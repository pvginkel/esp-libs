#include "support.h"

#include "ZigBeeAttribute.h"

#include "ZigBeeLock.h"

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

    return esp_zb_zcl_get_attribute(_endpoint_id, _cluster_id, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, _attribute_id);
}

void ZigBeeAttribute::raw_set(void* buffer) {
    ZigBeeLock lock;

    ESP_ZB_ERROR_CHECK(esp_zb_zcl_set_attribute_val(_endpoint_id, _cluster_id, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                                    _attribute_id, buffer, false));
}
