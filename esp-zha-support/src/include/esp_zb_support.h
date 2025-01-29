#pragma once

#include "ha/esp_zigbee_ha_standard.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ESP_ZB_ZCL_BASIC_POWER_SOURCE_UNKNOWN = 0x0,
    ESP_ZB_ZCL_BASIC_POWER_SOURCE_MAINS_SINGLE_PHASE = 0x1,
    ESP_ZB_ZCL_BASIC_POWER_SOURCE_MAINS_3_PHASE = 0x2,
    ESP_ZB_ZCL_BASIC_POWER_SOURCE_BATTERY = 0x3,
    ESP_ZB_ZCL_BASIC_POWER_SOURCE_DC_SOURCE = 0x4,
    ESP_ZB_ZCL_BASIC_POWER_SOURCE_EMERGENCY_MAINS_CONSTANTLY_POWERED = 0x5,
    ESP_ZB_ZCL_BASIC_POWER_SOURCE_EMERGENCY_MAINS_AND_TRANSFER_SWITCH = 0x6
} esp_zb_zcl_basic_power_source_t;

esp_err_t esp_zb_endpoint_set_identity(esp_zb_cluster_list_t *ep, const char *manufacturer_name,
                                       const char *model_identity);

esp_err_t esp_zb_basic_cluster_add_string_attr(esp_zb_attribute_list_t *attr_list, uint16_t attr_id,
                                               const char *value_p);

#ifdef NDEBUG
#define ESP_ZB_ERROR_CHECK(x)              \
    do {                                   \
        esp_zb_zcl_status_t err_rc_ = (x); \
        (void)sizeof(err_rc_);             \
    } while (0)
#elif defined(CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_SILENT)
#define ESP_ZB_ERROR_CHECK(x)                                 \
    do {                                                      \
        esp_zb_zcl_status_t err_rc_ = (x);                    \
        if (unlikely(err_rc_ != ESP_ZB_ZCL_STATUS_SUCCESS)) { \
            abort();                                          \
        }                                                     \
    } while (0)
#else
#define ESP_ZB_ERROR_CHECK(x)                                                                                  \
    do {                                                                                                       \
        esp_zb_zcl_status_t err_rc_ = (x);                                                                     \
        if (unlikely(err_rc_ != ESP_ZB_ZCL_STATUS_SUCCESS)) {                                                  \
            printf("ESP_ZB_ERROR_ASSERT failed");                                                              \
            printf(" at %p\n", __builtin_return_address(0));                                                   \
            printf("file: \"%s\" line %d\nfunc: %s\nexpression: %s\n", __FILE__, __LINE__, __ASSERT_FUNC, #x); \
            abort();                                                                                           \
        }                                                                                                      \
    } while (0)
#endif

#ifdef __cplusplus
};
#endif
