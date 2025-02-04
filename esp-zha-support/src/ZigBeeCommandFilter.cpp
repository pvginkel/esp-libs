#include "support.h"

#include "ZigBeeCommandFilter.h"

#include "esp_zb_support.h"
#include "esp_zigbee_core.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "zdo/esp_zigbee_zdo_common.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "zboss_api.h"
#include "zcl/zb_zcl_common.h"
#ifdef __cplusplus
};
#endif

#define EXTRACT_TRANSITION_TIME(type)                \
    {                                                \
        const auto req = (type*)zb_buf_begin(param); \
        _transition_time = req->transition_time;     \
        req->transition_time = 0;                    \
        break;                                       \
    }

esp_err_t ZigBeeTransitionTimeCommandFilter::prefilterCommand(uint8_t param) {
    zb_zcl_parsed_hdr_t cmd_info;
    ZB_ZCL_COPY_PARSED_HEADER(param, &cmd_info);

    // ESP_LOGD(TAG, "Prefilter command cluster %" PRIu16 " command %" PRIu8, cmd_info.cluster_id, cmd_info.cmd_id);

    switch (cmd_info.cluster_id) {
        case ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL:
            switch (cmd_info.cmd_id) {
                case ZB_ZCL_CMD_LEVEL_CONTROL_MOVE_TO_LEVEL:
                case ZB_ZCL_CMD_LEVEL_CONTROL_MOVE_TO_LEVEL_WITH_ON_OFF:
                    EXTRACT_TRANSITION_TIME(zb_zcl_level_control_move_to_level_req_t);

                case ZB_ZCL_CMD_LEVEL_CONTROL_STEP:
                case ZB_ZCL_CMD_LEVEL_CONTROL_STEP_WITH_ON_OFF:
                    EXTRACT_TRANSITION_TIME(zb_zcl_level_control_step_req_t);
            }
            break;

        case ZB_ZCL_CLUSTER_ID_COLOR_CONTROL:
            switch (cmd_info.cmd_id) {
                case ZB_ZCL_CMD_COLOR_CONTROL_MOVE_TO_HUE:
                    EXTRACT_TRANSITION_TIME(zb_zcl_color_control_move_to_hue_req_t);
                case ZB_ZCL_CMD_COLOR_CONTROL_MOVE_TO_SATURATION:
                    EXTRACT_TRANSITION_TIME(zb_zcl_color_control_move_to_saturation_req_t);
                case ZB_ZCL_CMD_COLOR_CONTROL_MOVE_TO_HUE_SATURATION:
                    EXTRACT_TRANSITION_TIME(zb_zcl_color_control_move_to_hue_saturation_req_t);
                case ZB_ZCL_CMD_COLOR_CONTROL_MOVE_TO_COLOR:
                    EXTRACT_TRANSITION_TIME(zb_zcl_color_control_move_to_color_req_t);
                case ZB_ZCL_CMD_COLOR_CONTROL_STEP_COLOR:
                    EXTRACT_TRANSITION_TIME(zb_zcl_color_control_step_color_req_t);
                case ZB_ZCL_CMD_COLOR_CONTROL_MOVE_TO_COLOR_TEMPERATURE:
                    EXTRACT_TRANSITION_TIME(zb_zcl_color_control_move_to_color_temperature_req_t);
                case ZB_ZCL_CMD_COLOR_CONTROL_ENHANCED_MOVE_TO_HUE:
                    EXTRACT_TRANSITION_TIME(zb_zcl_color_control_enhanced_move_to_hue_req_t);
                case ZB_ZCL_CMD_COLOR_CONTROL_ENHANCED_STEP_HUE:
                    EXTRACT_TRANSITION_TIME(zb_zcl_color_control_enhanced_step_hue_req_t);
                case ZB_ZCL_CMD_COLOR_CONTROL_ENHANCED_MOVE_TO_HUE_SATURATION:
                    EXTRACT_TRANSITION_TIME(zb_zcl_color_control_enhanced_move_to_hue_saturation_req_t);
                case ZB_ZCL_CMD_COLOR_CONTROL_STEP_COLOR_TEMPERATURE:
                    EXTRACT_TRANSITION_TIME(zb_zcl_color_control_step_color_temp_req_t);
            }
    }

    return ESP_OK;
}