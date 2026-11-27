#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"

// The configuration of a UDPReceiver.
struct UDPReceiverConfig {
    uint16_t port = 11106;
    // The receiver's task. Above the output's, so a burst of packets drains from lwIP's queue before it
    // overflows (Intercom's finding H5).
    UBaseType_t task_priority = 8;
    BaseType_t task_core = tskNO_AFFINITY;
};

// What a UDPReceiver has counted since it began.
struct UDPReceiverStats {
    // Datagrams received and handed on.
    uint32_t datagrams{};
    // Times the socket failed to open, to bind or to receive. Each closes it and opens it again a
    // second later.
    uint32_t errors{};
    // The least stack its task has had spare, in bytes.
    uint32_t stack_spare_bytes{};
};

// A UDP port on IPv4, read on its own task: each datagram goes to the handler, with its sender.
//
// The task opens the socket, binds it to the port on every interface, and blocks on it. A failure
// closes it and opens it again a second later, so a receiver begun before the network is up binds once
// it is.
class UDPReceiver {
public:
    // The largest datagram it takes: what fits a 1500-byte MTU. A longer one is cut to this.
    static constexpr size_t MAX_DATAGRAM = 1472;

    // A datagram and its sender, on the receiver's task. `data` is valid during the call only.
    using Handler = std::function<void(const sockaddr_in& sender, const uint8_t* data, size_t length)>;

private:
    UDPReceiverConfig _config;
    Handler _handler;
    uint8_t* _buffer{};
    TaskHandle_t _task{};
    std::atomic<uint32_t> _datagrams{};
    std::atomic<uint32_t> _errors{};

public:
    UDPReceiver() = default;
    UDPReceiver(const UDPReceiver&) = delete;
    UDPReceiver& operator=(const UDPReceiver&) = delete;

    // Starts the task, which opens the port.
    esp_err_t begin(const UDPReceiverConfig& config, Handler handler);

    bool is_started() const { return _task != nullptr; }
    UDPReceiverStats get_stats() const;
    const UDPReceiverConfig& get_config() const { return _config; }

private:
    void run();
    void receive();
};
