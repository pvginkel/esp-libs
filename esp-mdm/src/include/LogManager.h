#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include <atomic>
#include <string>
#include <vector>

#include "MQTTConnection.h"
#include "Mutex.h"
#include "Signal.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class LogManager {
    struct Message {
        char* buffer;

        explicit Message(char* buffer) : buffer(buffer) {}
    };

    static LogManager* _instance;
    static char* _buffer;

    MQTTConnection& _mqtt_connection;
    vprintf_like_t _default_log_handler{};
    Mutex _mutex;
    std::vector<Message> _messages;
    std::string _device_entity_id;
    std::atomic<bool> _shutting_down{};
    Signal _signal;
    TaskHandle_t _task_handle{};

    static int log_handler(const char* message, va_list va);

public:
    explicit LogManager(MQTTConnection& mqtt_connection);

    esp_err_t begin();
    void set_device_entity_id(const std::string& device_entity_id);

private:
    bool can_send();
    void task_loop();
    void publish_messages();
    size_t get_message_count();
};
