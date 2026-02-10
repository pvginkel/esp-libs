#include "support.h"

#include "SEN0626.h"

#include "DFRobot_GestureFaceDetection.h"

LOG_TAG(SEN0626);

#define SEN0626_I2C_ADDR 0x72

esp_err_t SEN0626::begin(int sda_pin, int scl_pin, int int_pin) {
    // 1. Create I2C master bus.
    i2c_master_bus_config_t bus_config = {
        .i2c_port = -1,
        .sda_io_num = (gpio_num_t)sda_pin,
        .scl_io_num = (gpio_num_t)scl_pin,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
    };
    bus_config.flags.enable_internal_pullup = true;
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &_bus_handle), TAG, "Failed to create I2C master bus");

    // 2. Add I2C device.
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SEN0626_I2C_ADDR,
        .scl_speed_hz = 100000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(_bus_handle, &dev_config, &_dev_handle), TAG,
                        "Failed to add I2C device");

    // 3. Construct driver and verify PID.
    _gfd = new DFRobot_GestureFaceDetection_I2C(_dev_handle);
    if (!_gfd->begin()) {
        ESP_LOGE(TAG, "Device not found (PID mismatch)");
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "Device found PID=0x%04X VID=0x%04X", _gfd->getPid(), _gfd->getVid());

    // 4. Configure thresholds from Kconfig.
    _gfd->setGestureDetectThres(CONFIG_SEN0626_GESTURE_THRESHOLD);
    _gfd->setFaceDetectThres(CONFIG_SEN0626_FACE_SCORE_THRESHOLD);
    _gfd->setDetectThres(CONFIG_SEN0626_DETECT_THRESHOLD);

    ESP_LOGI(TAG, "Thresholds: gesture=%d face_score=%d detect=%d",
             CONFIG_SEN0626_GESTURE_THRESHOLD, CONFIG_SEN0626_FACE_SCORE_THRESHOLD, CONFIG_SEN0626_DETECT_THRESHOLD);

    // 5. Configure interrupt GPIO (active-low).
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << int_pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&io_conf), TAG, "Failed to configure interrupt GPIO");

    // 6. Create background task.
    xTaskCreate([](void* self) { ((SEN0626*)self)->task_loop(); }, "sen0626", CONFIG_MAIN_TASK_STACK_SIZE, this, 2,
                &_task_handle);

    // 7. Install GPIO ISR service (may already be installed).
    esp_err_t isr_err = gpio_install_isr_service(0);
    if (isr_err != ESP_OK && isr_err != ESP_ERR_INVALID_STATE) {
        ESP_RETURN_ON_ERROR(isr_err, TAG, "Failed to install GPIO ISR service");
    }

    // 8. Add ISR handler.
    ESP_RETURN_ON_ERROR(gpio_isr_handler_add((gpio_num_t)int_pin, isr_handler, this), TAG,
                        "Failed to add ISR handler");

    ESP_LOGI(TAG, "Started: SDA=%d SCL=%d INT=%d", sda_pin, scl_pin, int_pin);

    return ESP_OK;
}

void IRAM_ATTR SEN0626::isr_handler(void* arg) {
    auto* self = (SEN0626*)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(self->_task_handle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void SEN0626::task_loop() {
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        uint16_t type = _gfd->getGestureType();
        uint16_t score = _gfd->getGestureScore();

        if (type >= 1 && type <= 5) {
            ESP_LOGI(TAG, "Gesture detected: type=%u score=%u", type, score);

            GestureEvent event = {type, score};
            _gesture_detected.queue(_queue, event);
        }
    }
}
