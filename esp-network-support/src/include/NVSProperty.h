#pragma once

#include "nvs_flash.h"

esp_err_t nvs_get_i1(nvs_handle_t c_handle, const char* key, bool* out_value);
esp_err_t nvs_set_i1(nvs_handle_t handle, const char* key, bool value);

esp_err_t nvs_get_f32(nvs_handle_t c_handle, const char* key, float* out_value);
esp_err_t nvs_set_f32(nvs_handle_t handle, const char* key, float value);

class NVSProperty {
    const char* _key;

protected:
    NVSProperty(const char* key) : _key(key) {}

public:
    const char* get_key() { return _key; }
};

#define NVSProperty_DECLARE(type, typename_uc)                          \
    class NVSProperty##typename_uc : public NVSProperty {               \
    public:                                                             \
        NVSProperty##typename_uc(const char* key) : NVSProperty(key) {} \
                                                                        \
        type get(nvs_handle_t handle, type default_value);              \
        void set(nvs_handle_t handle, type value);                      \
    }

NVSProperty_DECLARE(bool, I1);
NVSProperty_DECLARE(int8_t, I8);
NVSProperty_DECLARE(uint8_t, U8);
NVSProperty_DECLARE(int16_t, I16);
NVSProperty_DECLARE(uint16_t, U16);
NVSProperty_DECLARE(int32_t, I32);
NVSProperty_DECLARE(uint32_t, U32);
NVSProperty_DECLARE(float, F32);

#undef NVSProperty_DECLARE
