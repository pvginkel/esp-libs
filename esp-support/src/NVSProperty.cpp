#include "NVSProperty.h"

esp_err_t nvs_get_i1(nvs_handle_t c_handle, const char* key, bool* out_value) {
    int8_t real_value;
    const auto err = nvs_get_i8(c_handle, key, &real_value);
    if (err == ESP_OK) {
        *out_value = !!real_value;
    }
    return err;
}

esp_err_t nvs_set_i1(nvs_handle_t handle, const char* key, bool value) {
    return nvs_set_i8(handle, key, int8_t(value));
}

esp_err_t nvs_get_f32(nvs_handle_t c_handle, const char* key, float* out_value) {
    return nvs_get_u32(c_handle, key, (uint32_t*)out_value);
}

esp_err_t nvs_set_f32(nvs_handle_t handle, const char* key, float value) {
    return nvs_set_u32(handle, key, *(uint32_t*)&value);
}

#define NVSProperty_DEFINE(type, typename_uc, typename_lc)                        \
    type NVSProperty##typename_uc::get(nvs_handle_t handle, type default_value) { \
        type value;                                                               \
        const auto err = nvs_get_##typename_lc(handle, get_key(), &value);        \
        if (err != ESP_OK) {                                                      \
            value = default_value;                                                \
        }                                                                         \
        return value;                                                             \
    }                                                                             \
                                                                                  \
    void NVSProperty##typename_uc::set(nvs_handle_t handle, type value) {         \
        ESP_ERROR_CHECK(nvs_set_##typename_lc(handle, get_key(), value));         \
    }

NVSProperty_DEFINE(bool, I1, i1);
NVSProperty_DEFINE(int8_t, I8, i8);
NVSProperty_DEFINE(uint8_t, U8, u8);
NVSProperty_DEFINE(int16_t, I16, i16);
NVSProperty_DEFINE(uint16_t, U16, u16);
NVSProperty_DEFINE(int32_t, I32, i32);
NVSProperty_DEFINE(uint32_t, U32, u32);
NVSProperty_DEFINE(float, F32, f32);
