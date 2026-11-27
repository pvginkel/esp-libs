#pragma once

#include <cstddef>
#include <cstdint>

#include "driver/i2c_master.h"
#include "esp_err.h"

// Sensirion's SGP40 VOC sensor on I2C, at 0x59. It owns its bus: begin() creates an I2C master bus on
// the pins and adds the sensor to it, as SEN0626 does.
//
// Every call blocks the calling task for the sensor's measurement time, so a sampling task calls it
// rather than an event loop. VOCIndex turns the raw signal into the VOC index.
class SGP40 {
    i2c_master_bus_handle_t _bus_handle{};
    i2c_master_dev_handle_t _dev_handle{};

public:
    // The self-test's result word when every test passed.
    static constexpr uint16_t SELF_TEST_PASSED = 0xD400;

    SGP40() = default;
    ~SGP40();

    SGP40(const SGP40&) = delete;
    SGP40& operator=(const SGP40&) = delete;

    esp_err_t begin(int sda_pin, int scl_pin);

    // The sensor's 48-bit serial number.
    esp_err_t read_serial_number(uint64_t& serial_number);

    // Runs the built-in self-test, which takes 320 ms. ESP_OK when it passed and ESP_FAIL when it
    // failed, with result holding the result word.
    esp_err_t self_test(uint16_t& result);

    // One raw measurement, with the default compensation of 50 %RH and 25 °C. Takes 30 ms. Feed it to
    // VOCIndex once a second. It logs nothing on a failure, so a sensor that went away does not flood
    // the log once a second: the caller logs.
    esp_err_t measure_raw(uint16_t& raw);

private:
    esp_err_t command(const uint8_t* tx, size_t tx_len, uint32_t wait_ms, uint16_t* words, size_t word_count);
};
