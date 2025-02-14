#include "support.h"

#include "ZigBeeDevice.h"

#include "ZigBeeEndpoint.h"

LOG_TAG(ZigBeeDevice);

static constexpr uint32_t ZB_MAIN_TASK_STACK_SIZE = 6192;

static ZigBeeDevice *APP_INSTANCE = nullptr;

// Missing declaration.
extern "C" void zb_set_ed_node_descriptor(bool power_src, bool rx_on_when_idle, bool alloc_addr);

static bool edBatteryPowered = false;

ZigBeeDevice::ZigBeeDevice() {
    ESP_ERROR_ASSERT(!APP_INSTANCE);

    APP_INSTANCE = this;

    _radio_config.radio_mode = ZB_RADIO_MODE_NATIVE;                   // Use the native 15.4 radio
    _host_config.host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE;  // Disable host connection
    _zb_ep_list = esp_zb_ep_list_create();
}

esp_err_t ZigBeeDevice::begin(esp_zb_nwk_device_type_t role, bool erase_nvs) {
    switch (role) {
        case ESP_ZB_DEVICE_TYPE_COORDINATOR: {
            esp_zb_cfg_t role_cfg = ZIGBEE_DEFAULT_COORDINATOR_CONFIG();
            return begin(&role_cfg, erase_nvs);
        }
        case ESP_ZB_DEVICE_TYPE_ROUTER: {
            esp_zb_cfg_t role_cfg = ZIGBEE_DEFAULT_ROUTER_CONFIG();
            return begin(&role_cfg, erase_nvs);
        }
        case ESP_ZB_DEVICE_TYPE_ED: {
            esp_zb_cfg_t role_cfg = ZIGBEE_DEFAULT_ED_CONFIG();
            return begin(&role_cfg, erase_nvs);
        }
        default:
            ESP_LOGE(TAG, "Invalid ZigBee Role");
            return ESP_ERR_NOT_SUPPORTED;
    }
}

esp_err_t ZigBeeDevice::begin(esp_zb_cfg_t *zb_cfg, bool erase_nvs) {
    _role = zb_cfg->esp_zb_role;

    // ZigBee platform configuration
    esp_zb_platform_config_t platform_config = {
        .radio_config = _radio_config,
        .host_config = _host_config,
    };

    esp_err_t err = esp_zb_platform_config(&platform_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ZigBee platform");
        return false;
    }

    // Initialize ZigBee stack
    ESP_LOGD(TAG, "Initialize ZigBee stack");
    esp_zb_init(zb_cfg);

    // Register all ZigBee EPs in list
    if (_ep_objects.empty()) {
        ESP_LOGW(TAG, "No ZigBee EPs to register");
    } else {
        ESP_LOGD(TAG, "Register all ZigBee EPs in list");
        err = esp_zb_device_register(_zb_ep_list);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register ZigBee EPs");
            return false;
        }

        // print the list of ZigBee EPs from _ep_objects
        ESP_LOGI(TAG, "List of registered ZigBee EPs:");
        for (const auto ep_object : _ep_objects) {
            ESP_LOGI(TAG, "Endpoint: %d, Device ID: 0x%04x", ep_object->_endpoint_config.endpoint,
                     ep_object->_endpoint_config.app_device_id);
            if (ep_object->_battery_percentage_remaining) {
                edBatteryPowered = true;
            }
        }
    }
    // Register ZigBee action handler
    esp_zb_core_action_handler_register([](esp_zb_core_action_callback_id_t callback_id, const void *message) {
        return APP_INSTANCE->zbActionHandler(callback_id, message);
    });
    err = esp_zb_set_primary_network_channel_set(_primary_channel_mask);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set primary network channel mask");
        return false;
    }

    // Erase NVRAM before creating connection to new Coordinator
    if (erase_nvs) {
        esp_zb_nvram_erase_at_start(true);
    }

    // Create ZigBee task and start ZigBee stack
    xTaskCreate([](void *pvParameters) { ((ZigBeeDevice *)pvParameters)->zbTask(); }, "ZigBee_main",
                ZB_MAIN_TASK_STACK_SIZE, this, 5, NULL);

    return true;
}

void ZigBeeDevice::addEndpoint(ZigBeeEndpoint *ep) {
    _ep_objects.push_back(ep);

    ep->begin();

    ESP_ERROR_CHECK(esp_zb_ep_list_add_ep(_zb_ep_list, ep->_cluster_list, ep->_endpoint_config));
}

void ZigBeeDevice::startupComplete() {
    _started = true;
    _started_cb.call();
}

void ZigBeeDevice::zbTask() {
#if CONFIG_ZB_DEBUG_MODE
    // esp_zb_set_trace_level_mask(ESP_ZB_TRACE_LEVEL_INFO,
    //                             ESP_ZB_TRACE_SUBSYSTEM_APP | ESP_ZB_TRACE_SUBSYSTEM_ZCL |
    //                             ESP_ZB_TRACE_SUBSYSTEM_ZDO);
#endif

    for (const auto ep : _ep_objects) {
        auto const endpoint = zb_af_get_endpoint_desc(ep->_endpoint_config.endpoint);

        ep->_original_device_handler = endpoint->device_handler;
        endpoint->device_handler = [](zb_uint8_t param) { return APP_INSTANCE->zbDeviceHandler(param); };
    }

    esp_zb_bdb_set_scan_duration(_scan_duration);

    /* initialize ZigBee stack */
    ESP_ERROR_CHECK(esp_zb_start(false));

    // NOTE: This is a workaround to make battery powered devices to be discovered as battery powered
    if (_role == ESP_ZB_DEVICE_TYPE_ED && edBatteryPowered) {
        zb_set_ed_node_descriptor(0, _rx_on_when_idle, 1);
    }

    esp_zb_stack_main_loop();
}

void ZigBeeDevice::setRadioConfig(esp_zb_radio_config_t config) { _radio_config = config; }

esp_zb_radio_config_t ZigBeeDevice::getRadioConfig() { return _radio_config; }

void ZigBeeDevice::setHostConfig(esp_zb_host_config_t config) { _host_config = config; }

esp_zb_host_config_t ZigBeeDevice::getHostConfig() { return _host_config; }

void ZigBeeDevice::setPrimaryChannelMask(uint32_t mask) { _primary_channel_mask = mask; }

void ZigBeeDevice::setScanDuration(uint8_t duration) {
    ESP_ERROR_ASSERT(duration >= 1 && duration <= 4);
    _scan_duration = duration;
}

void ZigBeeDevice::setRebootOpenNetwork(uint8_t time) { _open_network = time; }

void ZigBeeDevice::openNetwork(uint8_t time) {
    ESP_ERROR_ASSERT(started());

    ESP_LOGI(TAG, "Opening network for joining for %d seconds", time);

    esp_zb_bdb_open_network(time);
}

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask) {
    ESP_ERROR_CHECK(esp_zb_bdb_start_top_level_commissioning(mode_mask));
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct) { APP_INSTANCE->zbSignalHandler(signal_struct); }

void ZigBeeDevice::zbSignalHandler(esp_zb_app_signal_t *signal_struct) {
    // common variables
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = (esp_zb_app_signal_type_t)*p_sg_p;
    // coordinator variables
    esp_zb_zdo_signal_device_annce_params_t *dev_annce_params = NULL;

    // main switch
    switch (sig_type) {
        case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:  // Common
            ESP_LOGI(TAG, "ZigBee stack initialized");
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
            break;

        case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:  // Common
        case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:       // Common
            if (err_status == ESP_OK) {
                ESP_LOGI(TAG, "Device started up in %s factory-reset mode", esp_zb_bdb_is_factory_new() ? "" : "non");
                if (esp_zb_bdb_is_factory_new()) {
                    // Role specific code
                    if (_role == ESP_ZB_DEVICE_TYPE_COORDINATOR) {
                        ESP_LOGI(TAG, "Start network formation");
                        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_FORMATION);
                    } else {
                        ESP_LOGI(TAG, "Start network steering");
                        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
                        startupComplete();
                    }
                } else {
                    ESP_LOGI(TAG, "Device rebooted");
                    startupComplete();
                    if (_role == ESP_ZB_DEVICE_TYPE_COORDINATOR && _open_network > 0) {
                        ESP_LOGI(TAG, "Opening network for joining for %d seconds", _open_network);
                        esp_zb_bdb_open_network(_open_network);
                    }
                    searchBindings();
                }
            } else {
                // Commissioning failed
                ESP_LOGW(TAG, "Commissioning failed (%s), trying again...", esp_err_to_name(err_status));
                esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb,
                                       ESP_ZB_BDB_MODE_INITIALIZATION, 500);
            }
            break;

        case ESP_ZB_BDB_SIGNAL_FORMATION:  // Coordinator
            if (_role == ESP_ZB_DEVICE_TYPE_COORDINATOR) {
                if (err_status == ESP_OK) {
                    esp_zb_ieee_addr_t extended_pan_id;
                    esp_zb_get_extended_pan_id(extended_pan_id);
                    ESP_LOGI(
                        TAG,
                        "Formed network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN "
                        "ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
                        extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                        extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                        esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
                    esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
                } else {
                    ESP_LOGI(TAG, "Restart network formation (status: %s)", esp_err_to_name(err_status));
                    esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb,
                                           ESP_ZB_BDB_MODE_NETWORK_FORMATION, 1000);
                }
            }
            break;

        case ESP_ZB_BDB_SIGNAL_STEERING:  // Router and End Device
            if (_role == ESP_ZB_DEVICE_TYPE_COORDINATOR) {
                if (err_status == ESP_OK) {
                    ESP_LOGI(TAG, "Network steering started");
                }
                startupComplete();
            } else {
                if (err_status == ESP_OK) {
                    esp_zb_ieee_addr_t extended_pan_id;
                    esp_zb_get_extended_pan_id(extended_pan_id);
                    ESP_LOGI(
                        TAG,
                        "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN "
                        "ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
                        extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                        extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                        esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
                    startupComplete();
                } else {
                    ESP_LOGI(TAG, "Network steering was not successful (status: %s)", esp_err_to_name(err_status));
                    esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb,
                                           ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
                }
            }
            break;

        case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE:  // Coordinator
            if (_role == ESP_ZB_DEVICE_TYPE_COORDINATOR) {
                dev_annce_params = (esp_zb_zdo_signal_device_annce_params_t *)esp_zb_app_signal_get_params(p_sg_p);

                ESP_LOGI(TAG, "New device commissioned or rejoined (short: 0x%04hx)",
                         dev_annce_params->device_short_addr);

                esp_zb_zdo_match_desc_req_param_t cmd_req{
                    .dst_nwk_addr = dev_annce_params->device_short_addr,
                    .addr_of_interest = dev_annce_params->device_short_addr,
                };

                ESP_LOGD(TAG, "Device capabilities: 0x%02x", dev_annce_params->capability);

                // capability:
                // Bit 0 – Alternate PAN Coordinator
                // Bit 1 – Device type: 1- ZigBee Router; 0 – End Device
                // Bit 2 – Power Source: 1 Main powered
                // Bit 3 – Receiver on when Idle
                // Bit 4 – Reserved
                // Bit 5 – Reserved
                // Bit 6 – Security capability
                // Bit 7 – Reserved

                // for each endpoint in the list call the findEndpoint function if not bounded or allowed to bind
                // multiple devices

                for (const auto ep_object : _ep_objects) {
                    if (!_allow_multiple_binding && !ep_object->_bound_devices.empty()) {
                        continue;
                    }

                    // Check if the device is already bound
                    bool found = false;
                    // Get the list of devices bound to the EP
                    for (const auto bound_device : ep_object->_bound_devices) {
                        if ((bound_device->short_addr == dev_annce_params->device_short_addr) ||
                            (memcmp(bound_device->ieee_addr, dev_annce_params->ieee_addr,
                                    sizeof(bound_device->ieee_addr)) == 0)) {
                            found = true;
                            ESP_LOGD(TAG, "Device already bound to endpoint %d", ep_object->getEndpoint());
                            break;
                        }
                    }
                    if (!found) {
                        ep_object->findEndpoint(&cmd_req);
                    }
                }
            }
            break;

        case ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS:  // Coordinator
            if (_role == ESP_ZB_DEVICE_TYPE_COORDINATOR) {
                if (err_status == ESP_OK) {
                    if (*(uint8_t *)esp_zb_app_signal_get_params(p_sg_p)) {
                        ESP_LOGI(TAG, "Network(0x%04hx) is open for %d seconds", esp_zb_get_pan_id(),
                                 *(uint8_t *)esp_zb_app_signal_get_params(p_sg_p));
                    } else {
                        ESP_LOGI(TAG, "Network(0x%04hx) closed, devices joining not allowed.", esp_zb_get_pan_id());
                    }
                }
            }
            break;

        case ESP_ZB_ZDO_SIGNAL_LEAVE:  // End Device + Router
            // Device was removed from the network, factory reset the device
            if (_role != ESP_ZB_DEVICE_TYPE_COORDINATOR) {
                factoryReset();
            }
            break;

        default:
            ESP_LOGD(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
                     esp_err_to_name(err_status));
            break;
    }
}

uint8_t ZigBeeDevice::zbDeviceHandler(uint8_t param) {
    zb_zcl_parsed_hdr_t cmd_info;
    ZB_MEMCPY(&cmd_info, ZB_BUF_GET_PARAM(param, zb_zcl_parsed_hdr_t), sizeof(zb_zcl_parsed_hdr_t));

    const auto dst_endpoint = ZB_ZCL_PARSED_HDR_SHORT_DATA(&cmd_info).dst_endpoint;

    for (const auto ep : _ep_objects) {
        if (ep->_endpoint_config.endpoint == dst_endpoint) {
            for (const auto command_filter : ep->_command_filters) {
                if (cmd_info.is_common_command) {
                    ESP_ERROR_CHECK(command_filter->prefilterCommonCommand(param));
                } else {
                    ESP_ERROR_CHECK(command_filter->prefilterSpecificCommand(param));
                }
            }

            if (ep->_original_device_handler) {
                return ep->_original_device_handler(param);
            } else {
                return 1;
            }
        }
    }

    ESP_LOGE(TAG, "Didn't find an endpoint");

    return 1;
}

void ZigBeeDevice::factoryReset() {
    ESP_LOGI(TAG, "Factory resetting ZigBee stack, device will reboot");

    esp_zb_factory_reset();
}

void ZigBeeDevice::scanCompleteCallback(esp_zb_zdp_status_t zdo_status, uint8_t count,
                                        esp_zb_network_descriptor_t *nwk_descriptor) {
    ESP_LOGI(TAG, "ZigBee network scan complete");

    _scan_result.clear();

    if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
        ESP_LOGD(TAG, "Found %d networks", count);

        // print ZigBee networks
        for (int i = 0; i < count; i++) {
            ESP_LOGD(TAG,
                     "Network %d: PAN ID: 0x%04hx, Permit Joining: %s, Extended PAN ID: "
                     "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, Channel: %d, Router Capacity: %s, End "
                     "Device Capacity: %s",
                     i, nwk_descriptor[i].short_pan_id, nwk_descriptor[i].permit_joining ? "Yes" : "No",
                     nwk_descriptor[i].extended_pan_id[7], nwk_descriptor[i].extended_pan_id[6],
                     nwk_descriptor[i].extended_pan_id[5], nwk_descriptor[i].extended_pan_id[4],
                     nwk_descriptor[i].extended_pan_id[3], nwk_descriptor[i].extended_pan_id[2],
                     nwk_descriptor[i].extended_pan_id[1], nwk_descriptor[i].extended_pan_id[0],
                     nwk_descriptor[i].logic_channel, nwk_descriptor[i].router_capacity ? "Yes" : "No",
                     nwk_descriptor[i].end_device_capacity ? "Yes" : "No");

            _scan_result.push_back(nwk_descriptor[i]);
        }

        // save scan result and update scan status
        // copy network descriptor to _scan_result to keep the data after the callback

        _scan_result.insert(_scan_result.end(), nwk_descriptor, nwk_descriptor + count);
        _scan_status = ZB_SCAN_SUCCESS;
    } else {
        ESP_LOGE(TAG, "Failed to scan ZigBee network (status: 0x%x)", zdo_status);

        _scan_status = ZB_SCAN_FAILED;
    }
}

void ZigBeeDevice::scanNetworks(uint32_t channel_mask, uint8_t scan_duration) {
    if (!started()) {
        ESP_LOGE(TAG, "ZigBee stack is not started, cannot scan networks");
        return;
    }

    ESP_LOGI(TAG, "Scanning ZigBee networks");

    esp_zb_zdo_active_scan_request(
        channel_mask, scan_duration,
        [](esp_zb_zdp_status_t zdo_status, uint8_t count, esp_zb_network_descriptor_t *nwk_descriptor) {
            APP_INSTANCE->scanCompleteCallback(zdo_status, count, nwk_descriptor);
        });

    _scan_status = ZB_SCAN_RUNNING;
}

// Recall bounded devices from the binding table after reboot
void ZigBeeDevice::bindingTableCb(const esp_zb_zdo_binding_table_info_t *table_info,
                                  esp_zb_zdo_mgmt_bind_param_t *mb_req) {
    bool done = true;
    esp_zb_zdp_status_t zdo_status = (esp_zb_zdp_status_t)table_info->status;
    ESP_LOGD(TAG, "Binding table callback for address 0x%04x with status %d", mb_req->dst_addr, zdo_status);
    if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
        // Print binding table log simple
        ESP_LOGD(TAG, "Binding table info: total %d, index %d, count %d", table_info->total, table_info->index,
                 table_info->count);

        if (table_info->total == 0) {
            ESP_LOGD(TAG, "No binding table entries found");
            free(mb_req);
            return;
        }

        esp_zb_zdo_binding_table_record_t *record = table_info->record;
        for (int i = 0; i < table_info->count; i++) {
            ESP_LOGD(TAG, "Binding table record: src_endp %d, dst_endp %d, cluster_id 0x%04x, dst_addr_mode %d",
                     record->src_endp, record->dst_endp, record->cluster_id, record->dst_addr_mode);

            zb_device_params_t *device = (zb_device_params_t *)calloc(1, sizeof(zb_device_params_t));
            device->endpoint = record->dst_endp;
            if (record->dst_addr_mode == ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT ||
                record->dst_addr_mode == ESP_ZB_APS_ADDR_MODE_16_GROUP_ENDP_NOT_PRESENT) {
                device->short_addr = record->dst_address.addr_short;
            } else {  // ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT
                memcpy(device->ieee_addr, record->dst_address.addr_long, sizeof(esp_zb_ieee_addr_t));
            }

            // Add to list of bound devices of proper endpoint
            for (const auto ep_object : _ep_objects) {
                if (ep_object->getEndpoint() == record->src_endp) {
                    ep_object->_bound_devices.push_back(device);
                    ESP_LOGD(TAG,
                             "Device bound to EP %d -> device endpoint: %d, short addr: 0x%04x, ieee addr: "
                             "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
                             record->src_endp, device->endpoint, device->short_addr, device->ieee_addr[7],
                             device->ieee_addr[6], device->ieee_addr[5], device->ieee_addr[4], device->ieee_addr[3],
                             device->ieee_addr[2], device->ieee_addr[1], device->ieee_addr[0]);
                }
            }
            record = record->next;
        }

        // Continue reading the binding table
        if (table_info->index + table_info->count < table_info->total) {
            /* There are unreported binding table entries, request for them. */
            mb_req->start_index = table_info->index + table_info->count;
            esp_zb_zdo_binding_table_req(
                mb_req,
                [](const esp_zb_zdo_binding_table_info_t *table_info, void *user_ctx) {
                    APP_INSTANCE->bindingTableCb(table_info, (esp_zb_zdo_mgmt_bind_param_t *)user_ctx);
                },
                mb_req);
            done = false;
        }
    }

    if (done) {
        // Print bound devices
        ESP_LOGD(TAG, "Filling bounded devices finished");
        free(mb_req);
    }
}

void ZigBeeDevice::searchBindings() {
    esp_zb_zdo_mgmt_bind_param_t *mb_req = (esp_zb_zdo_mgmt_bind_param_t *)malloc(sizeof(esp_zb_zdo_mgmt_bind_param_t));
    mb_req->dst_addr = esp_zb_get_short_address();
    mb_req->start_index = 0;

    ESP_LOGD(TAG, "Requesting binding table for address 0x%04x", mb_req->dst_addr);

    esp_zb_zdo_binding_table_req(
        mb_req,
        [](const esp_zb_zdo_binding_table_info_t *table_info, void *user_ctx) {
            APP_INSTANCE->bindingTableCb(table_info, (esp_zb_zdo_mgmt_bind_param_t *)user_ctx);
        },
        (void *)mb_req);
}

esp_err_t ZigBeeDevice::zbActionHandler(esp_zb_core_action_callback_id_t callback_id, const void *message) {
    switch (callback_id) {
        case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
            return zbAttributeSetHandler((esp_zb_zcl_set_attr_value_message_t *)message);
        case ESP_ZB_CORE_REPORT_ATTR_CB_ID:
            return zbAttributeReportingHandler((esp_zb_zcl_report_attr_message_t *)message);
        case ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID:
            return zbCommandReadAttributeResponseHandler((esp_zb_zcl_cmd_read_attr_resp_message_t *)message);
        case ESP_ZB_CORE_CMD_REPORT_CONFIG_RESP_CB_ID:
            return zbConfigureReportingResponseHandler((esp_zb_zcl_cmd_config_report_resp_message_t *)message);
        case ESP_ZB_CORE_CMD_DEFAULT_RESP_CB_ID:
            return zbCommandDefaultResponseHandler((esp_zb_zcl_cmd_default_resp_message_t *)message);
        default:
            ESP_LOGW(TAG, "Receive unhandled ZigBee action(0x%x) callback", callback_id);
            return ESP_OK;
    }
}

esp_err_t ZigBeeDevice::zbAttributeSetHandler(const esp_zb_zcl_set_attr_value_message_t *message) {
    if (!message) {
        ESP_LOGE(TAG, "Empty message");
        return ESP_FAIL;
    }
    if (message->info.status != ESP_ZB_ZCL_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Received attribute set: error status(%d)", message->info.status);
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGD(TAG, "Received attribute set: endpoint(%d), cluster(0x%x), attribute(0x%x), data size(%d)",
             message->info.dst_endpoint, message->info.cluster, message->attribute.id, message->attribute.data.size);

    // List through all ZigBee EPs and call the callback function, with the message
    for (const auto ep_object : _ep_objects) {
        if (message->info.dst_endpoint == ep_object->getEndpoint()) {
            if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY) {
                ep_object->zbIdentify(message);
            } else {
                ep_object->zbAttributeSet(message);
            }
        }
    }
    return ESP_OK;
}

esp_err_t ZigBeeDevice::zbAttributeReportingHandler(const esp_zb_zcl_report_attr_message_t *message) {
    if (!message) {
        ESP_LOGE(TAG, "Empty message");
        return ESP_FAIL;
    }
    if (message->status != ESP_ZB_ZCL_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Received attribute report: error status(%d)", message->status);
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGD(TAG, "Received attribute report from address(0x%x) src endpoint(%d) to dst endpoint(%d) cluster(0x%x)",
             message->src_address.u.short_addr, message->src_endpoint, message->dst_endpoint, message->cluster);
    // List through all ZigBee EPs and call the callback function, with the message
    for (const auto ep_object : _ep_objects) {
        if (message->dst_endpoint == ep_object->getEndpoint()) {
            ep_object->zbAttributeRead(message->cluster, &message->attribute);
        }
    }
    return ESP_OK;
}

esp_err_t ZigBeeDevice::zbCommandReadAttributeResponseHandler(const esp_zb_zcl_cmd_read_attr_resp_message_t *message) {
    if (!message) {
        ESP_LOGE(TAG, "Empty message");
        return ESP_FAIL;
    }
    if (message->info.status != ESP_ZB_ZCL_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Received attribute response: error status(%d)", message->info.status);
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGD(TAG, "Read attribute response: from address(0x%x) src endpoint(%d) to dst endpoint(%d) cluster(0x%x)",
             message->info.src_address.u.short_addr, message->info.src_endpoint, message->info.dst_endpoint,
             message->info.cluster);

    for (const auto ep_object : _ep_objects) {
        if (message->info.dst_endpoint == ep_object->getEndpoint()) {
            for (auto variable = message->variables; variable; variable = variable->next) {
                ESP_LOGD(TAG,
                         "Read attribute response: status(%d), cluster(0x%x), attribute(0x%x), type(0x%x), value(%d)",
                         variable->status, message->info.cluster, variable->attribute.id, variable->attribute.data.type,
                         variable->attribute.data.value ? *(uint8_t *)variable->attribute.data.value : 0);

                if (variable->status == ESP_ZB_ZCL_STATUS_SUCCESS) {
                    ep_object->zbAttributeRead(message->info.cluster, &variable->attribute);
                }
            }
        }
    }

    return ESP_OK;
}

esp_err_t ZigBeeDevice::zbConfigureReportingResponseHandler(
    const esp_zb_zcl_cmd_config_report_resp_message_t *message) {
    if (!message) {
        ESP_LOGE(TAG, "Empty message");
        return ESP_FAIL;
    }
    if (message->info.status != ESP_ZB_ZCL_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Received configure report response: error status(%d)", message->info.status);
        return ESP_ERR_INVALID_ARG;
    }

    for (auto variable = message->variables; variable; variable = variable->next) {
        ESP_LOGD(TAG, "Received configure report response: status(%d), cluster(0x%x), direction(0x%x), attribute(0x%x)",
                 variable->status, message->info.cluster, variable->direction, variable->attribute_id);
    }

    return ESP_OK;
}

esp_err_t ZigBeeDevice::zbCommandDefaultResponseHandler(const esp_zb_zcl_cmd_default_resp_message_t *message) {
    if (!message) {
        ESP_LOGE(TAG, "Empty message");
        return ESP_FAIL;
    }
    if (message->info.status != ESP_ZB_ZCL_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Received message: error status(%d)", message->info.status);
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGD(TAG,
             "Received default response: from address(0x%x), src_endpoint(%d) to dst_endpoint(%d), cluster(0x%x) with "
             "status 0x%x",
             message->info.src_address.u.short_addr, message->info.src_endpoint, message->info.dst_endpoint,
             message->info.cluster, message->status_code);

    return ESP_OK;
}
