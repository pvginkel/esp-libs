#pragma once

#include "Callback.h"
#include "esp_zb_support.h"

class ZigBeeEndpoint;

class ZigBeeAttribute {
    friend ZigBeeEndpoint;

    uint8_t _endpoint_id;
    uint16_t _cluster_id;
    uint16_t _attribute_id;
    esp_zb_zcl_attr_type_t _attr_type;

public:
    ZigBeeAttribute(uint8_t endpoint_id, uint16_t cluster_id, uint16_t attribute_id, esp_zb_zcl_attr_type_t attr_type)
        : _endpoint_id(endpoint_id), _cluster_id(cluster_id), _attribute_id(attribute_id), _attr_type(attr_type) {}

    void report();

protected:
    virtual void raiseChanged(const esp_zb_zcl_set_attr_value_message_t* message) = 0;

    esp_zb_zcl_attr_t* doGet();
    void doSet(void* buffer);
    esp_err_t doAddToCluster(esp_zb_attribute_list_t* attr_list,
                             std::function<esp_err_t(esp_zb_attribute_list_t*, uint16_t, void*)> func, void* value_p) {
        return func(attr_list, _attribute_id, value_p);
    }
    esp_err_t doAddToCluster(esp_zb_attribute_list_t* attr_list, uint8_t attr_access,
                             std::function<esp_err_t(esp_zb_attribute_list_t*, uint16_t, uint8_t, uint8_t, void*)> func,
                             void* value_p) {
        return func(attr_list, _attribute_id, (uint8_t)_attr_type, attr_access, value_p);
    }
};

class ZigBeeAttributeU8 : public ZigBeeAttribute {
    Callback<uint8_t> _changed;

public:
    ZigBeeAttributeU8(uint8_t endpoint_id, uint16_t cluster_id, uint16_t attribute_id,
                      esp_zb_zcl_attr_type_t attr_type = ESP_ZB_ZCL_ATTR_TYPE_U8)
        : ZigBeeAttribute(endpoint_id, cluster_id, attribute_id, attr_type) {}

    uint8_t get();
    uint8_t get(uint8_t default_value);
    void set(uint8_t value) { doSet(&value); }
    void onChanged(std::function<void(uint8_t)> func) { _changed.add(func); }

    esp_err_t addToCluster(esp_zb_attribute_list_t* attr_list,
                           std::function<esp_err_t(esp_zb_attribute_list_t*, uint16_t, void*)> func, uint8_t value) {
        return doAddToCluster(attr_list, func, &value);
    }

    esp_err_t addToCluster(esp_zb_attribute_list_t* attr_list, uint8_t attr_access,
                           std::function<esp_err_t(esp_zb_attribute_list_t*, uint16_t, uint8_t, uint8_t, void*)> func,
                           uint8_t value) {
        return doAddToCluster(attr_list, attr_access, func, &value);
    }

protected:
    void raiseChanged(const esp_zb_zcl_set_attr_value_message_t* message) override {
        _changed.call(*(uint8_t*)message->attribute.data.value);
    }
};

class ZigBeeAttributeU16 : public ZigBeeAttribute {
    Callback<uint16_t> _changed;

public:
    ZigBeeAttributeU16(uint8_t endpoint_id, uint16_t cluster_id, uint16_t attribute_id,
                       esp_zb_zcl_attr_type_t attr_type = ESP_ZB_ZCL_ATTR_TYPE_U16)
        : ZigBeeAttribute(endpoint_id, cluster_id, attribute_id, attr_type) {}

    uint16_t get();
    uint16_t get(uint16_t default_value);
    void set(uint16_t value) { doSet(&value); }
    void onChanged(std::function<void(uint16_t)> func) { _changed.add(func); }

    esp_err_t addToCluster(esp_zb_attribute_list_t* attr_list,
                           std::function<esp_err_t(esp_zb_attribute_list_t*, uint16_t, void*)> func, uint16_t value) {
        return doAddToCluster(attr_list, func, &value);
    }

    esp_err_t addToCluster(esp_zb_attribute_list_t* attr_list, uint8_t attr_access,
                           std::function<esp_err_t(esp_zb_attribute_list_t*, uint16_t, uint8_t, uint8_t, void*)> func,
                           uint16_t value) {
        return doAddToCluster(attr_list, attr_access, func, &value);
    }

protected:
    void raiseChanged(const esp_zb_zcl_set_attr_value_message_t* message) override {
        _changed.call(*(uint16_t*)message->attribute.data.value);
    }
};
