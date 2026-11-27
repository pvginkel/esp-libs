#include "support.h"

#include "UDPReceiver.h"

#include <cerrno>
#include <cinttypes>

#include "defer.h"
#include "esp_heap_caps.h"

LOG_TAG(UDPReceiver);

// The receive loop and the handler, which for AudioPlayer places a packet in the stream buffer.
static constexpr uint32_t TASK_STACK_SIZE = 4096;

// How long a failed socket waits before it opens again.
static constexpr uint32_t RETRY_DELAY_MS = 1000;

esp_err_t UDPReceiver::begin(const UDPReceiverConfig& config, Handler handler) {
    ESP_ASSERT_RETURN(!_task, ESP_ERR_INVALID_STATE);
    ESP_ASSERT_RETURN(handler && config.port > 0, ESP_ERR_INVALID_ARG);

    _config = config;
    _handler = std::move(handler);

    _buffer = (uint8_t*)heap_caps_malloc(MAX_DATAGRAM, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_ASSERT_RETURN(_buffer, ESP_ERR_NO_MEM);

    const auto created = xTaskCreatePinnedToCore([](void* arg) { ((UDPReceiver*)arg)->run(); }, "udp_receiver",
                                                 TASK_STACK_SIZE, this, config.task_priority, &_task, config.task_core);
    ESP_ASSERT_RETURN(created == pdPASS, ESP_ERR_NO_MEM);

    return ESP_OK;
}

UDPReceiverStats UDPReceiver::get_stats() const {
    return {
        .datagrams = _datagrams,
        .errors = _errors,
        .stack_spare_bytes = _task ? (uint32_t)uxTaskGetStackHighWaterMark(_task) : 0,
    };
}

void UDPReceiver::run() {
    while (true) {
        receive();

        _errors++;

        ESP_LOGW(TAG, "Opening port %u again in %" PRIu32 " ms", _config.port, RETRY_DELAY_MS);

        vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
    }
}

// Opens the socket and hands on what it receives, until it fails.
void UDPReceiver::receive() {
    const auto sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Opening a socket failed: errno %d", errno);
        return;
    }
    DEFER(close(sock));

    const sockaddr_in address = {
        .sin_len = sizeof(sockaddr_in),
        .sin_family = AF_INET,
        .sin_port = htons(_config.port),
        .sin_addr = {.s_addr = htonl(INADDR_ANY)},
        .sin_zero = {},
    };

    if (bind(sock, (const sockaddr*)&address, sizeof(address)) < 0) {
        ESP_LOGE(TAG, "Binding port %u failed: errno %d", _config.port, errno);
        return;
    }

    ESP_LOGI(TAG, "Listening on UDP port %u", _config.port);

    while (true) {
        sockaddr_in sender{};
        socklen_t sender_length = sizeof(sender);

        const auto length = recvfrom(sock, _buffer, MAX_DATAGRAM, 0, (sockaddr*)&sender, &sender_length);
        if (length < 0) {
            ESP_LOGE(TAG, "Receiving on port %u failed: errno %d", _config.port, errno);
            return;
        }

        _datagrams++;

        if (sender.sin_family == AF_INET) {
            _handler(sender, _buffer, (size_t)length);
        }
    }
}
