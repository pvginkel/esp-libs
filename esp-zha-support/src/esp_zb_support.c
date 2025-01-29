#include "esp_zb_support.h"
#include "support.h"

esp_err_t esp_zb_endpoint_set_identity(esp_zb_cluster_list_t *ep, const char *manufacturer_name,
                                       const char *model_identity) {
    esp_zb_attribute_list_t *basic_cluster =
        esp_zb_cluster_list_get_cluster(ep, ESP_ZB_ZCL_CLUSTER_ID_BASIC, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    esp_err_t err = esp_zb_basic_cluster_add_string_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID,
                                                         manufacturer_name);
    if (err != ESP_OK) {
        return err;
    }

    return esp_zb_basic_cluster_add_string_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID,
                                                model_identity);
}

esp_err_t esp_zb_basic_cluster_add_string_attr(esp_zb_attribute_list_t *attr_list, uint16_t attr_id,
                                               const char *value_p) {
    size_t len = strlen(value_p);
    ESP_ERROR_ASSERT(len < 256);
    char *copy = (char *)malloc(len + 2);
    copy[0] = len;
    memcpy(copy + 1, value_p, len + 1);

    esp_err_t err = esp_zb_basic_cluster_add_attr(attr_list, attr_id, copy);

    free(copy);

    return err;
}
