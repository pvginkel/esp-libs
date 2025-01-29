#include "support.h"

#include "ZigBeeEndpoint.h"

#include "esp_zigbee_cluster.h"
#include "zcl/esp_zigbee_zcl_power_config.h"

LOG_TAG(ZigBeeEndpoint);

// TODO: is_bound and allow_multiple_binding to make not static

void ZigBeeEndpoint::begin() {
    auto cluster_config = buildCluster();

    _cluster_list = cluster_config.cluster_list;

    ESP_LOGD(TAG, "Endpoint: %d, Device ID: 0x%04x", _endpoint_config.endpoint, _endpoint_config.app_device_id);

    esp_zb_endpoint_set_identity(_cluster_list, cluster_config.manufacturer_name, cluster_config.model_identifier);

    esp_zb_attribute_list_t *basic_cluster =
        esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_BASIC, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_update_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID,
                               (void *)&cluster_config.power_source);

    if (cluster_config.power_source == ESP_ZB_ZCL_BASIC_POWER_SOURCE_BATTERY) {
        // Call to esp_zb_power_config_cluster_add_attr is missing here. Is this necessary?
        _battery_percentage_remaining = createAttributeU8(ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
                                                          ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID);
    }
}

ZigBeeAttributeBool *ZigBeeEndpoint::createAttributeBool(uint16_t cluster_id, uint16_t attribute_id) {
    const auto attribute = new ZigBeeAttributeBool(getEndpoint(), cluster_id, attribute_id);
    _attributes.push_back(attribute);
    return attribute;
}

ZigBeeAttributeU8 *ZigBeeEndpoint::createAttributeU8(uint16_t cluster_id, uint16_t attribute_id) {
    const auto attribute = new ZigBeeAttributeU8(getEndpoint(), cluster_id, attribute_id);
    _attributes.push_back(attribute);
    return attribute;
}

ZigBeeAttributeU16 *ZigBeeEndpoint::createAttributeU16(uint16_t cluster_id, uint16_t attribute_id) {
    const auto attribute = new ZigBeeAttributeU16(getEndpoint(), cluster_id, attribute_id);
    _attributes.push_back(attribute);
    return attribute;
}

void ZigBeeEndpoint::setBatteryPercentage(uint8_t percentage) {
    ESP_ERROR_ASSERT(_battery_percentage_remaining);

    // 100% = 200 in decimal, 0% = 0
    // Convert percentage to 0-200 range
    if (percentage > 100) {
        percentage = 100;
    }

    _battery_percentage_remaining->set(percentage * 2);
    _battery_percentage_remaining->report();

    ESP_LOGD(TAG, "Battery percentage updated");
}

void ZigBeeEndpoint::printBoundDevices() {
    ESP_LOGI(TAG, "Bound devices:");
    for (const auto &device : _bound_devices) {
        ESP_LOGI(TAG,
                 "Device on endpoint %d, short address: 0x%x, ieee address: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
                 device->endpoint, device->short_addr, device->ieee_addr[7], device->ieee_addr[6], device->ieee_addr[5],
                 device->ieee_addr[4], device->ieee_addr[3], device->ieee_addr[2], device->ieee_addr[1],
                 device->ieee_addr[0]);
    }
}

void ZigBeeEndpoint::zbIdentify(const esp_zb_zcl_set_attr_value_message_t *message) {
    if (message->attribute.id == ESP_ZB_ZCL_CMD_IDENTIFY_IDENTIFY_ID &&
        message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
        _identify.call(*(uint16_t *)message->attribute.data.value);
    } else {
        ESP_LOGW(TAG, "Other identify commands are not implemented yet.");
    }
}
