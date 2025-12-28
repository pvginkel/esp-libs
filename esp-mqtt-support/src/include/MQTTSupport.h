#pragma once

enum class SwitchState { ON, OFF, UNKNOWN };

SwitchState parse_switch_state(const char* value);
const char* print_switch_state(SwitchState state);
