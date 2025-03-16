#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include <string>
#include <vector>

#include "Mutex.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"

class LogManager {
    struct Message {
        char* buffer;
        uint32_t time;

        Message(char* buffer, uint32_t time) : buffer(buffer), time(time) {}
    };

    static LogManager* _instance;
    static char* _buffer;

    vprintf_like_t _default_log_handler{};
    Mutex _mutex;
    std::vector<Message> _messages;
    const char* _device_entity_id{};
    esp_timer_handle_t _log_timer;

    static int log_handler(const char* message, va_list va);

public:
    LogManager();

    void begin();
    void set_device_entity_id(const char* device_entity_id);

private:
    void upload_logs();
    void start_timer();
};
