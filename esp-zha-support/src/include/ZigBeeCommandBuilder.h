#pragma once

#include <functional>

#include "ZigBeeLock.h"
#include "ZigBeeStream.h"
#include "ha/esp_zigbee_ha_standard.h"

class ZigBeeCommandBuilder {
    static uint8_t* _buffer;

    esp_zb_zcl_custom_cluster_cmd_req_t cmd_req{};

public:
    ZigBeeCommandBuilder& withSourceEndpoint(uint8_t src_endpoint) {
        cmd_req.zcl_basic_cmd.src_endpoint = src_endpoint;
        return *this;
    }
    ZigBeeCommandBuilder& withDestinationEndpoint(uint8_t dst_endpoint) {
        cmd_req.zcl_basic_cmd.dst_endpoint = dst_endpoint;
        return *this;
    }
    ZigBeeCommandBuilder& withDestinationAddress(esp_zb_addr_u dst_addr_u) {
        cmd_req.zcl_basic_cmd.dst_addr_u = dst_addr_u;
        return *this;
    }
    ZigBeeCommandBuilder& withAddressMode(esp_zb_zcl_address_mode_t address_mode) {
        cmd_req.address_mode = address_mode;
        return *this;
    }
    ZigBeeCommandBuilder& withProfileId(uint16_t profile_id) {
        cmd_req.profile_id = profile_id;
        return *this;
    }
    ZigBeeCommandBuilder& withClusterId(uint16_t cluster_id) {
        cmd_req.cluster_id = cluster_id;
        return *this;
    }
    ZigBeeCommandBuilder& withManufacturerSpecific(uint8_t manuf_specific) {
        cmd_req.manuf_specific = manuf_specific;
        return *this;
    }
    ZigBeeCommandBuilder& withDirection(uint8_t direction) {
        cmd_req.direction = direction;
        return *this;
    }
    ZigBeeCommandBuilder& withDisableDefaultResponse(uint8_t dis_defalut_resp) {
        cmd_req.dis_defalut_resp = dis_defalut_resp;
        return *this;
    }
    ZigBeeCommandBuilder& withManufacturerCode(uint16_t manuf_code) {
        cmd_req.manuf_code = manuf_code;
        return *this;
    }
    ZigBeeCommandBuilder& withCustomCommandId(uint16_t custom_cmd_id) {
        cmd_req.custom_cmd_id = custom_cmd_id;
        return *this;
    }

    void send();
    void send(std::function<void(ZigBeeStream&)> buffer_writer);
};
