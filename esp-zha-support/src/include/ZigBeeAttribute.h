#pragma once

#include "esp_zb_support.h"

#include "Callback.h"

class ZigBeeEndpoint;

class ZigBeeAttribute {
    friend ZigBeeEndpoint;

    uint8_t _endpoint_id;
    uint16_t _cluster_id;
    uint16_t _attribute_id;

public:
    ZigBeeAttribute(uint8_t endpoint_id, uint16_t cluster_id, uint16_t attribute_id)
        : _endpoint_id(endpoint_id), _cluster_id(cluster_id), _attribute_id(attribute_id) {}

    void report();

protected:
    virtual void raise_changed(const esp_zb_zcl_set_attr_value_message_t* message) = 0;

    esp_zb_zcl_attr_t* raw_get();
    void raw_set(void* buffer);
};

class ZigBeeAttributeBool : public ZigBeeAttribute {
    Callback<bool> _changed;

public:
    ZigBeeAttributeBool(uint8_t endpoint_id, uint16_t cluster_id, uint16_t attribute_id)
        : ZigBeeAttribute(endpoint_id, cluster_id, attribute_id) {}

    bool get() { return *(bool*)raw_get()->data_p; }
    void set(bool value) { raw_set(&value); }
    void on_changed(std::function<void(bool)> func) { _changed.add(func); }

protected:
    void raise_changed(const esp_zb_zcl_set_attr_value_message_t* message) override {
        _changed.call(*(bool*)message->attribute.data.value);
    }
};

class ZigBeeAttributeU8 : public ZigBeeAttribute {
    Callback<uint8_t> _changed;

public:
    ZigBeeAttributeU8(uint8_t endpoint_id, uint16_t cluster_id, uint16_t attribute_id)
        : ZigBeeAttribute(endpoint_id, cluster_id, attribute_id) {}

    uint8_t get() { return *(uint8_t*)raw_get()->data_p; }
    void set(uint8_t value) { raw_set(&value); }
    void on_changed(std::function<void(uint8_t)> func) { _changed.add(func); }

protected:
    void raise_changed(const esp_zb_zcl_set_attr_value_message_t* message) override {
        _changed.call(*(uint8_t*)message->attribute.data.value);
    }
};

class ZigBeeAttributeU16 : public ZigBeeAttribute {
    Callback<uint8_t> _changed;

public:
    ZigBeeAttributeU16(uint8_t endpoint_id, uint16_t cluster_id, uint16_t attribute_id)
        : ZigBeeAttribute(endpoint_id, cluster_id, attribute_id) {}

    uint16_t get() { return *(uint16_t*)raw_get()->data_p; }
    void set(uint16_t value) { raw_set(&value); }
    void on_changed(std::function<void(uint16_t)> func) { _changed.add(func); }

protected:
    void raise_changed(const esp_zb_zcl_set_attr_value_message_t* message) override {
        _changed.call(*(uint16_t*)message->attribute.data.value);
    }
};
