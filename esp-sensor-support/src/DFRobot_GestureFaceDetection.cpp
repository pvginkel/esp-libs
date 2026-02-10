/*!
 *@file DFRobot_GestureFaceDetection.cpp
 *@brief Define the basic structure of class DFRobot_GestureFaceDetection, the implementation of basic methods.
 *@details this module is used to identify the information in the QR code
 *@copyright   Copyright (c) 2025 DFRobot Co.Ltd (http://www.dfrobot.com)
 *@License     The MIT License (MIT)
 *@author [thdyyl](yuanlong.yu@dfrobot.com)
 *@version  V1.0
 *@date  2025-04-02
 *@https://github.com/DFRobot/DFRobot_GestureFaceDetection
 */

#include "DFRobot_GestureFaceDetection.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

DFRobot_GestureFaceDetection::DFRobot_GestureFaceDetection()
{
}
bool DFRobot_GestureFaceDetection::begin()
{
    if (reaInputdReg(REG_GFD_PID) == GFD_PID)
    {
        return true;
    }
    return false;
}
uint16_t DFRobot_GestureFaceDetection::getPid()
{
    return reaInputdReg(REG_GFD_PID);
}
uint16_t DFRobot_GestureFaceDetection::getVid()
{
    return reaInputdReg(REG_GFD_VID);
}

uint16_t DFRobot_GestureFaceDetection::getFaceNumber()
{
    return reaInputdReg(REG_GFD_FACE_NUMBER);
}
uint16_t DFRobot_GestureFaceDetection::configUart(eBaudConfig_t baud, eParityConfig_t parity, eStopbits_t stopBit)
{
    uint16_t ret = 0;
    if(baud < eBaud_1200 || baud >= eBaud_MAX){
        ret |= ERR_INVALID_BAUD;
    }
    if(static_cast<uint8_t>(parity) >= UART_CFG_PARITY_MAX){
        ret |= ERR_INVALID_PARITY;
    }
    if(static_cast<uint8_t>(stopBit) >= UART_CFG_STOP_MAX){
        ret |= ERR_INVALID_STOPBIT;
    }
    if(ret != 0){
        return ret;
    }
    uint16_t baudRate = baud;

    uint16_t verifyAndStop = ((uint16_t)parity << 8) | ((uint16_t)stopBit & 0xff);
    ret |= writeIHoldingReg(REG_GFD_BAUDRATE, baudRate)? SUCCESS : ERR_CONFIG_BUAD;
    ret |= writeIHoldingReg(REG_GFD_VERIFY_AND_STOP, verifyAndStop)? SUCCESS: ERR_CONFIG_PARITY_STOPBIT;
    return ret;
}

uint16_t DFRobot_GestureFaceDetection::getFaceLocationX()
{
    return reaInputdReg(REG_GFD_FACE_LOCATION_X);
}

uint16_t DFRobot_GestureFaceDetection::getFaceLocationY()
{
    return reaInputdReg(REG_GFD_FACE_LOCATION_Y);
}
uint16_t DFRobot_GestureFaceDetection::getFaceScore()
{
    return reaInputdReg(REG_GFD_FACE_SCORE);
}
uint16_t DFRobot_GestureFaceDetection::getGestureType()
{
    return reaInputdReg(REG_GFD_GESTURE_TYPE);
}
uint16_t DFRobot_GestureFaceDetection::getGestureScore()
{

    return reaInputdReg(REG_GFD_GESTURE_SCORE);
}

bool DFRobot_GestureFaceDetection::setFaceDetectThres(uint16_t score)
{
    if (score > 100)
    {
        return false;
    }
    return writeIHoldingReg(REG_GFD_FACE_SCORE_THRESHOLD, score);
}
uint16_t DFRobot_GestureFaceDetection::getFaceDetectThres()
{
    return readHoldingReg(REG_GFD_FACE_SCORE_THRESHOLD);
}
bool DFRobot_GestureFaceDetection::setDetectThres(uint16_t x)
{
    if (x > 100)
    {
        return false;
    }
    return writeIHoldingReg(REG_GFD_FACE_THRESHOLD, x);
}
uint16_t DFRobot_GestureFaceDetection::getDetectThres(){
    return readHoldingReg(REG_GFD_FACE_THRESHOLD);
}
bool DFRobot_GestureFaceDetection::setGestureDetectThres(uint16_t score)
{
    if (score > 100)
    {
        return false;
    }
    return writeIHoldingReg(REG_GFD_GESTURE_SCORE_THRESHOLD, score);
}
uint16_t DFRobot_GestureFaceDetection::getGestureDetectThres(){
    return readHoldingReg(REG_GFD_GESTURE_SCORE_THRESHOLD);
}
bool DFRobot_GestureFaceDetection::setDeviceAddr(uint16_t addr)
{
    if ((addr == 0) || (addr > 0xF7))
    {
        return false;
    }
    return writeIHoldingReg(REG_GFD_ADDR, addr);
}

DFRobot_GestureFaceDetection_I2C::DFRobot_GestureFaceDetection_I2C(i2c_master_dev_handle_t dev_handle)
    : _dev_handle(dev_handle)
{
}

bool DFRobot_GestureFaceDetection_I2C::begin()
{
    return DFRobot_GestureFaceDetection::begin();
}
uint8_t DFRobot_GestureFaceDetection_I2C::calculate_crc(const uint8_t *data, size_t length)
{
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < length; i++)
    {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x80)
            {
                crc = (crc << 1) ^ 0x07;
            }
            else
            {
                crc <<= 1;
            }
        }
        crc &= 0xFF;
    }
    return crc;
}

bool DFRobot_GestureFaceDetection_I2C::writeReg(uint16_t reg, uint16_t data)
{
    const uint8_t max_retry = 3;
    uint8_t crc_datas[] = {(uint8_t)(reg >> 8),
                           (uint8_t)(reg & 0xFF),
                           (uint8_t)(data >> 8),
                           (uint8_t)(data & 0xFF)};
    uint8_t crc = calculate_crc(crc_datas, 4);
    uint8_t tx_buf[] = {crc_datas[0], crc_datas[1], crc_datas[2], crc_datas[3], crc};

    for (uint8_t retry = 0; retry < max_retry; retry++)
    {
        if (i2c_master_transmit(_dev_handle, tx_buf, sizeof(tx_buf), -1) != ESP_OK)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(70));

        uint8_t rx_buf[3];
        if (i2c_master_receive(_dev_handle, rx_buf, sizeof(rx_buf), -1) != ESP_OK)
        {
            continue;
        }

        uint8_t re_crc = calculate_crc(rx_buf, 2);
        if (re_crc != rx_buf[2] || ((rx_buf[0] << 8) | rx_buf[1]) != crc)
        {
            continue;
        }

        return true;
    }

    return false;
}
uint16_t DFRobot_GestureFaceDetection_I2C::readReg(uint16_t reg)
{
    const uint8_t max_retry = 3;
    uint8_t crc_datas[] = {(uint8_t)(reg >> 8),
                           (uint8_t)(reg & 0xFF)};
    uint8_t crc = calculate_crc(crc_datas, 2);
    uint8_t tx_buf[] = {crc_datas[0], crc_datas[1], crc};

    for (uint8_t retry = 0; retry < max_retry; retry++)
    {
        if (i2c_master_transmit(_dev_handle, tx_buf, sizeof(tx_buf), -1) != ESP_OK)
        {
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(5));

        uint8_t rx_buf[3];
        if (i2c_master_receive(_dev_handle, rx_buf, sizeof(rx_buf), -1) != ESP_OK)
        {
            continue;
        }

        uint8_t re_crc = calculate_crc(rx_buf, 2);
        uint16_t value = (rx_buf[0] << 8) | rx_buf[1];
        if (value == 0xFFFF || re_crc != rx_buf[2])
        {
            continue;
        }

        return value;
    }
    return 0xFFFF;
}
bool DFRobot_GestureFaceDetection_I2C::writeIHoldingReg(uint16_t reg, uint16_t data)
{

    return writeReg(reg, data);
}
uint16_t DFRobot_GestureFaceDetection_I2C::reaInputdReg(uint16_t reg)
{
    return readReg(INPUT_REG_OFFSET + reg);
}

uint16_t DFRobot_GestureFaceDetection_I2C::readHoldingReg(uint16_t reg)
{
    return readReg(reg);
}
