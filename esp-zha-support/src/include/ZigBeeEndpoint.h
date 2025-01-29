#pragma once

#include "Callback.h"
#include "ZigBeeAttribute.h"
#include "ZigBeeDevice.h"
#include "esp_zigbee_core.h"

/* Useful defines */
#define ZB_CMD_TIMEOUT 10000  // 10 seconds

#define ZB_ARRAY_LENTH(arr) (sizeof(arr) / sizeof(arr[0]))

typedef struct zbstring_s {
    uint8_t len;
    char data[];
} ESP_ZB_PACKED_STRUCT zbstring_t;

typedef struct zb_device_params_s {
    esp_zb_ieee_addr_t ieee_addr;
    uint8_t endpoint;
    uint16_t short_addr;
} zb_device_params_t;

typedef struct {
    esp_zb_cluster_list_t *cluster_list;
    const char *manufacturer_name;
    const char *model_identifier;
    esp_zb_zcl_basic_power_source_t power_source;
} zb_endpoint_cluster_t;

/* ZigBee End Device Class */
class ZigBeeEndpoint {
    Callback<uint16_t> _identify;
    esp_zb_endpoint_config_t _endpoint_config;
    esp_zb_cluster_list_t *_cluster_list{};
    std::vector<zb_device_params_t *> _bound_devices;
    std::vector<ZigBeeAttribute *> _attributes;
    ZigBeeAttributeU8 *_battery_percentage_remaining;

public:
    ZigBeeEndpoint(esp_zb_endpoint_config_t endpoint_config) : _endpoint_config(endpoint_config) {}
    virtual ~ZigBeeEndpoint() {}

    uint8_t getEndpoint() { return _endpoint_config.endpoint; }
    void printBoundDevices();
    void setBatteryPercentage(uint8_t percentage);
    void onIdentify(std::function<void(uint16_t)> func) { _identify.add(func); }

protected:
    virtual zb_endpoint_cluster_t buildCluster() = 0;

    // findEndpoint may be implemented by EPs to find and bind devices
    virtual void findEndpoint(esp_zb_zdo_match_desc_req_param_t *cmd_req) {};

    // list of all handlers function calls, to be override by EPs implementation
    virtual void zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) {};
    virtual void zbAttributeRead(uint16_t cluster_id, const esp_zb_zcl_attribute_t *attribute) {};
    virtual void zbIdentify(const esp_zb_zcl_set_attr_value_message_t *message);

    ZigBeeAttributeBool *createAttributeBool(uint16_t cluster_id, uint16_t attribute_id);
    ZigBeeAttributeU8 *createAttributeU8(uint16_t cluster_id, uint16_t attribute_id);
    ZigBeeAttributeU16 *createAttributeU16(uint16_t cluster_id, uint16_t attribute_id);

private:
    void begin();

    friend class ZigBeeDevice;
};
