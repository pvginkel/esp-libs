#include "support.h"

#include "NetworkConnection.h"

#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"
#include "sdkconfig.h"
#include "strformat.h"

// TODO: REMOVE!
#define CONFIG_WIFI_SSID "Thing-Fish"

LOG_TAG(NetworkConnection);

NetworkConnection* NetworkConnection::_instance = nullptr;

NetworkConnection::NetworkConnection(Queue* synchronizationQueue) : _synchronization_queue(synchronizationQueue) {
    _instance = this;
}

esp_err_t NetworkConnection::begin(const char* password) {
    _wifi_event_group = xEventGroupCreate();

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK) {
        return err;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK) {
        return err;
    }

    _wifi_interface = esp_netif_create_default_wifi_sta();
    if (_wifi_interface == nullptr) {
        return ESP_FAIL;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        return err;
    }

    esp_event_handler_instance_t instanceAnyId;
    esp_event_handler_instance_t instanceGotIp;

    err = esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID,
        [](auto eventHandlerArg, auto eventBase, auto eventId, auto eventData) {
            ((NetworkConnection*)eventHandlerArg)->event_handler(eventBase, eventId, eventData);
        },
        this, &instanceAnyId);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP,
        [](auto eventHandlerArg, auto eventBase, auto eventId, auto eventData) {
            ((NetworkConnection*)eventHandlerArg)->event_handler(eventBase, eventId, eventData);
        },
        this, &instanceGotIp);
    if (err != ESP_OK) {
        return err;
    }

    wifi_config_t wifiConfig = {
        .sta =
            {
                .ssid = CONFIG_WIFI_SSID,

                // Authmode threshold resets to WPA2 as default if password
                // matches WPA2 standards (pasword len => 8). If you want to
                // connect the device to deprecated WEP/WPA networks, Please set
                // the threshold value to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and
                // set the password with length and format matching to
                // WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.

                .threshold =
                    {
                        .authmode = WIFI_AUTH_WPA2_PSK,
                    },
                .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
                .sae_h2e_identifier = "",
            },
    };

    strcpy((char*)wifiConfig.sta.password, password);

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_wifi_set_config(WIFI_IF_STA, &wifiConfig);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_wifi_start();
    if (err != ESP_OK) {
        return err;
    }

    if (CONFIG_WIFI_MAX_TX_POWER) {
        int8_t power;
        err = esp_wifi_get_max_tx_power(&power);
        if (err != ESP_OK) {
            return err;
        }

        const int8_t new_power = (int8_t)CONFIG_WIFI_MAX_TX_POWER;

        ESP_LOGI(TAG, "WiFi power set to %" PRIi8 " changing to %" PRIi8, power, new_power);

        err = esp_wifi_set_max_tx_power(new_power);
        if (err != ESP_OK) {
            return err;
        }
    }

    ESP_LOGI(TAG, "Finished setting up WiFi");

    return ESP_OK;
}

std::string NetworkConnection::get_ip_address() {
    if (_wifi_interface && esp_netif_is_netif_up(_wifi_interface)) {
        esp_netif_ip_info_t ip_info;
        ESP_ERROR_CHECK(esp_netif_get_ip_info(_wifi_interface, &ip_info));

        return strformat(IPSTR, IP2STR(&ip_info.ip));
    }

    return {};
}

void NetworkConnection::event_handler(esp_event_base_t eventBase, int32_t eventId, void* eventData) {
    if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Connecting to AP, attempt %d", _attempt + 1);
        esp_wifi_connect();
    } else if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_DISCONNECTED) {
        auto event = (wifi_event_sta_disconnected_t*)eventData;

        // Reasons are defined here:
        // https://github.com/espressif/esp-idf/blob/4b5b064caf951a48b48a39293d625b5de2132719/components/esp_wifi/include/esp_wifi_types_generic.h#L79.

        ESP_LOGW(TAG, "Disconnected from AP, reason %d", event->reason);

        if (_attempt++ < CONFIG_WIFI_CONNECT_ATTEMPTS) {
            ESP_LOGI(TAG, "Retrying...");
            esp_wifi_connect();
        } else {
            _state_changed.queue(_synchronization_queue, {.connected = false, .errorReason = event->reason});
        }
    } else if (eventBase == IP_EVENT && eventId == IP_EVENT_STA_GOT_IP) {
        auto event = (ip_event_got_ip_t*)eventData;

        ESP_LOGI(TAG, "Got ip:" IPSTR, IP2STR(&event->ip_info.ip));

        setup_sntp();
    }
}

void NetworkConnection::setup_sntp() {
#if CONFIG_ENABLE_SNTP

    ESP_LOGI(TAG, "Initializing SNTP");

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");

    config.sync_cb = [](struct timeval* tv) {
        tm time_info;
        localtime_r(&tv->tv_sec, &time_info);

        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &time_info);
        ESP_LOGI(TAG, "Time synchronized to %s", time_str);

        if (!_instance->_have_sntp_synced) {
            _instance->_have_sntp_synced = true;

            _instance->_state_changed.queue(_instance->_synchronization_queue, {.connected = true, .errorReason = 0});
        }
    };

    esp_netif_sntp_init(&config);

#else

    _instance->_state_changed.queue(_instance->_synchronization_queue, {.connected = true, .errorReason = 0});

#endif
}
