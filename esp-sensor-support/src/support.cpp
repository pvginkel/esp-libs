#include "support.h"

#include "soc/soc_caps.h"

bool adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t* out_handle) {
    *out_handle = nullptr;
    esp_err_t ret;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cfg = {
        .unit_id = unit,
        .atten = atten,
        .bitwidth = (adc_bitwidth_t)SOC_ADC_DIGI_MAX_BITWIDTH,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cfg, out_handle);
    if (ret == ESP_OK) {
        return true;
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cfg = {
        .unit_id = unit,
        .atten = atten,
        .bitwidth = (adc_bitwidth_t)SOC_ADC_DIGI_MAX_BITWIDTH,
    };
    ret = adc_cali_create_scheme_line_fitting(&cfg, out_handle);
    if (ret == ESP_OK) {
        return true;
    }
#endif

    return false;
}
