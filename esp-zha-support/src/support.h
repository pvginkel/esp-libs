#pragma once

#include "sdkconfig.h"
#include "soc/soc_caps.h"

#if !SOC_IEEE802154_SUPPORTED || !CONFIG_ZB_ENABLED
#error IEEE 802.15.4 must be supported and ZigBee must be enabled Please review the sdkconfig.defaults in this project.
#endif

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "driver/gpio.h"
#include "error.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_zigbee_core.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LOG_TAG(v) [[maybe_unused]] static const char *TAG = #v
