#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include <atomic>
#include <string>
#include <vector>

#include "Mutex.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"

class LogManager {
    struct Message {
        char* buffer;
        int64_t time;

        Message(char* buffer, int64_t time) : buffer(buffer), time(time) {}
    };

    static LogManager* _instance;
    static char* _buffer;

    std::string _logging_url;
    vprintf_like_t _default_log_handler{};
    Mutex _mutex;
    std::vector<Message> _messages;
    std::string _device_entity_id;
    esp_timer_handle_t _log_timer;
    std::atomic<bool> _shutting_down;
    esp_http_client_handle_t _client{};

    static int log_handler(const char* message, va_list va);

public:
    LogManager();

    esp_err_t begin(const std::string& logging_url);
    void set_device_entity_id(const std::string& device_entity_id);

private:
    esp_err_t upload_logs();
    void start_timer();
    esp_http_client_handle_t get_or_create_client();
    void reset_client();
};
