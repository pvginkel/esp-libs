#include "support.h"

#include "MQTTSupport.h"

LOG_TAG(MQTTSupport);

SwitchState parse_switch_state(const char* value) {
    if (strcmp(value, "on") == 0) {
        return SwitchState::ON;
    }
    if (strcmp(value, "off") == 0) {
        return SwitchState::OFF;
    }
    ESP_LOGW(TAG, "Cannot parse switch state '%s'", value);
    return SwitchState::UNKNOWN;
}
