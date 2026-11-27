#include "support.h"

#include "SGP40.h"

LOG_TAG(SGP40);

static constexpr uint16_t SGP40_I2C_ADDR = 0x59;
static constexpr int I2C_TIMEOUT_MS = 100;

// The commands and their maximum durations, from the datasheet.
static constexpr uint8_t CMD_GET_SERIAL_NUMBER[] = {0x36, 0x82};
static constexpr uint32_t GET_SERIAL_NUMBER_MS = 1;
static constexpr uint8_t CMD_EXECUTE_SELF_TEST[] = {0x28, 0x0E};
static constexpr uint32_t EXECUTE_SELF_TEST_MS = 320;
static constexpr uint32_t MEASURE_RAW_SIGNAL_MS = 30;

// Sensirion's CRC-8: polynomial 0x31, init 0xFF.
static constexpr uint8_t sensirion_crc(const uint8_t* data, size_t len) {
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc = (uint8_t)(crc ^ data[i]);
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

static constexpr uint8_t DEFAULT_HUMIDITY[] = {0x80, 0x00};     // 50 %RH
static constexpr uint8_t DEFAULT_TEMPERATURE[] = {0x66, 0x66};  // 25 °C

// The raw measurement with the default compensation, each parameter word with its CRC.
static constexpr uint8_t CMD_MEASURE_RAW_SIGNAL[] = {
    0x26,
    0x0F,
    DEFAULT_HUMIDITY[0],
    DEFAULT_HUMIDITY[1],
    sensirion_crc(DEFAULT_HUMIDITY, sizeof(DEFAULT_HUMIDITY)),
    DEFAULT_TEMPERATURE[0],
    DEFAULT_TEMPERATURE[1],
    sensirion_crc(DEFAULT_TEMPERATURE, sizeof(DEFAULT_TEMPERATURE)),
};

// Waits at least ms: vTaskDelay waits up to a tick less than it is given.
static void wait_at_least_ms(uint32_t ms) { vTaskDelay((ms + portTICK_PERIOD_MS - 1) / portTICK_PERIOD_MS + 1); }

SGP40::~SGP40() {
    if (_dev_handle) {
        i2c_master_bus_rm_device(_dev_handle);
    }
    if (_bus_handle) {
        i2c_del_master_bus(_bus_handle);
    }
}

esp_err_t SGP40::begin(int sda_pin, int scl_pin) {
    i2c_master_bus_config_t bus_config = {
        .i2c_port = -1,
        .sda_io_num = (gpio_num_t)sda_pin,
        .scl_io_num = (gpio_num_t)scl_pin,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
    };
    bus_config.flags.enable_internal_pullup = true;
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &_bus_handle), TAG, "Failed to create I2C master bus");

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SGP40_I2C_ADDR,
        .scl_speed_hz = 100000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(_bus_handle, &dev_config, &_dev_handle), TAG,
                        "Failed to add I2C device");

    ESP_LOGI(TAG, "Started: SDA=%d SCL=%d", sda_pin, scl_pin);

    return ESP_OK;
}

esp_err_t SGP40::read_serial_number(uint64_t& serial_number) {
    uint16_t words[3];
    ESP_RETURN_ON_ERROR(command(CMD_GET_SERIAL_NUMBER, sizeof(CMD_GET_SERIAL_NUMBER), GET_SERIAL_NUMBER_MS, words, 3),
                        TAG, "Failed to read the serial number");

    serial_number = (uint64_t)words[0] << 32 | (uint64_t)words[1] << 16 | words[2];

    return ESP_OK;
}

esp_err_t SGP40::self_test(uint16_t& result) {
    ESP_RETURN_ON_ERROR(command(CMD_EXECUTE_SELF_TEST, sizeof(CMD_EXECUTE_SELF_TEST), EXECUTE_SELF_TEST_MS, &result, 1),
                        TAG, "Failed to run the self-test");

    return result == SELF_TEST_PASSED ? ESP_OK : ESP_FAIL;
}

esp_err_t SGP40::measure_raw(uint16_t& raw) {
    return command(CMD_MEASURE_RAW_SIGNAL, sizeof(CMD_MEASURE_RAW_SIGNAL), MEASURE_RAW_SIGNAL_MS, &raw, 1);
}

// Sends the command, waits its duration, then reads the words it answers, each followed by its CRC. It
// logs nothing, because measure_raw runs once a second: a sensor that went away would flood the log.
esp_err_t SGP40::command(const uint8_t* tx, size_t tx_len, uint32_t wait_ms, uint16_t* words, size_t word_count) {
    uint8_t rx[9];
    if (!_dev_handle) {
        return ESP_ERR_INVALID_STATE;
    }
    if (word_count * 3 > sizeof(rx)) {
        return ESP_ERR_INVALID_SIZE;
    }

    auto err = i2c_master_transmit(_dev_handle, tx, tx_len, I2C_TIMEOUT_MS);
    if (err != ESP_OK) {
        return err;
    }

    wait_at_least_ms(wait_ms);

    err = i2c_master_receive(_dev_handle, rx, word_count * 3, I2C_TIMEOUT_MS);
    if (err != ESP_OK) {
        return err;
    }

    for (size_t i = 0; i < word_count; i++) {
        const auto word = &rx[i * 3];
        if (sensirion_crc(word, 2) != word[2]) {
            return ESP_ERR_INVALID_CRC;
        }
        words[i] = (uint16_t)(word[0] << 8 | word[1]);
    }

    return ESP_OK;
}
