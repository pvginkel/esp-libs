#pragma once

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "driver/gpio.h"
#include "error.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_continuous.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/adc_types.h"
#include "sdkconfig.h"

#define LOG_TAG(v) [[maybe_unused]] static const char* TAG = #v

bool adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t* out_handle);
