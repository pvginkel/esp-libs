#include "support.h"

int hextoi(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    return -1;
}

char const* esp_reset_reason_to_name(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_POWERON:
            return "ESP_RST_POWERON";
        case ESP_RST_EXT:
            return "ESP_RST_EXT";
        case ESP_RST_SW:
            return "ESP_RST_SW";
        case ESP_RST_PANIC:
            return "ESP_RST_PANIC";
        case ESP_RST_INT_WDT:
            return "ESP_RST_INT_WDT";
        case ESP_RST_TASK_WDT:
            return "ESP_RST_TASK_WDT";
        case ESP_RST_WDT:
            return "ESP_RST_WDT";
        case ESP_RST_DEEPSLEEP:
            return "ESP_RST_DEEPSLEEP";
        case ESP_RST_BROWNOUT:
            return "ESP_RST_BROWNOUT";
        case ESP_RST_SDIO:
            return "ESP_RST_SDIO";
        default:
            return "ESP_RST_UNKNOWN";
    }
}
