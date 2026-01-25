/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
#ifndef FINGERPRINT_H
#define FINGERPRINT_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class Fingerprint
{
public:
    // 构造函数
    Fingerprint(int rx_pin, int tx_pin);

    // 初始化函数
    void begin(uint32_t baud_rate = 57600);

    // 开启和关闭电源
    void setPower(bool on);

    // 等待指纹模组启动信号
    bool waitStartSignal();

    // 读取模组基本参数
    bool readInfo();

    // 注册指纹
    bool registerFingerprint(int template_id = 0);

    // 搜索指纹
    bool searchFingerprint();
    bool autoIdentifyFingerprint();

    // 删除指定指纹
    bool deleteFingerprint(uint16_t id);
    // 清空指纹库
    bool clearAllLib();

    // 读取有效模板数量
    int readValidTempleteNum();

    // 读取索引表
    bool readIndexTable(uint8_t *indexTable);

    // 休眠
    bool sleepFingerprint();

    // 呼吸灯自动手动切换
    bool setLEDAutoManual(int autoMode);
    // 呼吸灯指令
    bool setLEDCmd(uint8_t code, uint8_t startColor, uint8_t endColor, uint8_t loopCount);
    bool setLEDCmd(uint8_t code, uint8_t startColor, uint8_t endColorOrdutyCicle, uint8_t loopCount,uint8_t time);
    // 打印十六进制数据
    void printHex(uint8_t *data, uint8_t len);

     // 呼吸灯指令 - 模式
    static const uint8_t LED_MODE_AUTO = 0xFF;        // 呼吸灯自动模式
    static const uint8_t LED_MODE_MANUAL = 0x00;      // 呼吸灯手动模式
    // 呼吸灯指令 - 功能码
    static const uint8_t LED_CODE_BREATH = 0x01;
    static const uint8_t LED_CODE_BLINK = 0x02;
    static const uint8_t LED_CODE_ON = 0x03;
    static const uint8_t LED_CODE_OFF = 0x04;
    static const uint8_t LED_CODE_SLOW_ON = 0x05;
    static const uint8_t LED_CODE_SLOW_OFF = 0x06;
    static const uint8_t LED_CODE_MARQUEE = 0x07;
    // 呼吸灯指令 - 颜色码
    static const uint8_t LED_ALL_OFF = 0x00;
    static const uint8_t LED_ALL_ON = 0x07;
    static const uint8_t LED_BLUE_ON = 0x01;
    static const uint8_t LED_GREEN_ON = 0x02;
    static const uint8_t LED_RED_ON = 0x04;
    static const uint8_t LED_RED_GREEN_ON = 0x06;
    static const uint8_t LED_RED_BLUE_ON = 0x05;
    static const uint8_t LED_GREEN_BLUE_ON = 0x03;
private:
    // 定义指令包格式
    static const uint8_t HEADER_HIGH = 0xEF;
    static const uint8_t HEADER_LOW = 0x01;
    static const uint32_t DEVICE_ADDRESS = 0xFFFFFFFF;

    // 定义指令码
    static const uint8_t CMD_GET_IMAGE = 0x01;             // 获取图像
    static const uint8_t CMD_GEN_CHAR = 0x02;              // 生成特征
    static const uint8_t CMD_MATCH = 0x03;                 // 精确比对指纹
    static const uint8_t CMD_SEARCH = 0x04;                // 搜索指纹
    static const uint8_t CMD_REG_MODEL = 0x05;             // 合并特征
    static const uint8_t CMD_STORE_CHAR = 0x06;            // 存储模板
    static const uint8_t CMD_DELETE_CHAR = 0x0C;            // 存储模板
    static const uint8_t CMD_CLEAR_LIB = 0x0D;              // 清空指纹库
    static const uint8_t CMD_READ_SYSPARA = 0x0F;           // 读模组基本参数
    static const uint8_t CMD_AUTO_IDENTIFY = 0x32;          // 自动验证指纹
    static const uint8_t CMD_VALID_TEMPLATE_NUM = 0x1D;     // 读取有效模版数
    static const uint8_t CMD_READ_INDEX = 0x1F;             // 读取索引表
    static const uint8_t CMD_SLEEP = 0x33;                  // 休眠指令
    static const uint8_t CMD_LED_AUTO_MANUAL = 0x60;        // 呼吸灯自动手动切换
    static const uint8_t CMD_LED_CM = 0x3C;                 // 呼吸灯指令

    // 成员变量
    int _rx_pin;
    int _tx_pin;
    uint8_t _buffer_id;
    SemaphoreHandle_t _mutex; // 互斥锁

    // 私有方法
    void sendCmd12(uint8_t cmd);
    void sendCmd13(uint8_t cmd, uint8_t param1);
    void sendCmd15(uint8_t cmd, uint8_t param1, uint16_t param2);
    void sendCmd16(uint8_t cmd, uint8_t param1, uint16_t param2);
    void sendCmd16(uint8_t cmd, uint8_t param1, uint16_t param2, uint16_t param3, uint16_t param4);
    void sendCmd17(uint8_t cmd, uint8_t param1, uint16_t param2, uint16_t param3);
    void sendCmd17(uint8_t cmd, uint8_t param1, uint8_t param2, uint8_t param3, uint8_t param4, uint8_t param5);
    bool receiveResponse();
    bool receiveResponse(int &data);
    bool receiveIndexTable(uint8_t* data);
    void printResponse(uint8_t *response, uint8_t length);
};


#endif // FINGERPRINT_H