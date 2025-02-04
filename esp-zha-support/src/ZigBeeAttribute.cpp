#include "support.h"

#include "ZigBeeAttribute.h"

#include "ZigBeeLock.h"

LOG_TAG(ZigBeeAttribute);

void ZigBeeAttribute::report() {
    // Send report attributes command

    esp_zb_zcl_report_attr_cmd_t report_attr_cmd = {
        .zcl_basic_cmd =
            {
                .src_endpoint = _endpoint_id,
            },
        .address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT,
        .clusterID = _cluster_id,
        .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI,
        .attributeID = _attribute_id,
    };

    ZigBeeLock lock;

    esp_zb_zcl_report_attr_cmd_req(&report_attr_cmd);
}

esp_zb_zcl_attr_t* ZigBeeAttribute::raw_get() {
    ZigBeeLock lock;

    const auto attr =
        esp_zb_zcl_get_attribute(_endpoint_id, _cluster_id, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, _attribute_id);
    if (!attr) {
        ESP_LOGE(TAG, "Cannot find attribute with cluster %" PRIu16 " ID %" PRIu16, _cluster_id, _attribute_id);
        ESP_ERROR_ASSERT(attr);
    }
    return attr;
}

void ZigBeeAttribute::raw_set(void* buffer) {
    ZigBeeLock lock;

    ESP_ZB_ERROR_CHECK(esp_zb_zcl_set_attribute_val(_endpoint_id, _cluster_id, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                                    _attribute_id, buffer, false));
}

bool ZigBeeAttributeBool::get() {
    const auto data_p = (bool*)raw_get()->data_p;
    ESP_ERROR_ASSERT(data_p);
    return *data_p;
}

bool ZigBeeAttributeBool::get(bool default_value) {
    const auto data_p = (bool*)raw_get()->data_p;
    return data_p ? *data_p : default_value;
}

uint8_t ZigBeeAttributeU8::get() {
    const auto data_p = (uint8_t*)raw_get()->data_p;
    ESP_ERROR_ASSERT(data_p);
    return *data_p;
}

uint8_t ZigBeeAttributeU8::get(uint8_t default_value) {
    const auto data_p = (uint8_t*)raw_get()->data_p;
    return data_p ? *data_p : default_value;
}

uint16_t ZigBeeAttributeU16::get() {
    const auto data_p = (uint16_t*)raw_get()->data_p;
    ESP_ERROR_ASSERT(data_p);
    return *data_p;
}

uint16_t ZigBeeAttributeU16::get(uint16_t default_value) {
    const auto data_p = (uint16_t*)raw_get()->data_p;
    return data_p ? *data_p : default_value;
}
