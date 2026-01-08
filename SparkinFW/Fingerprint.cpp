/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
#include "Fingerprint.h"
#include "Common.h"
#include "IOPin.h"
#include "BluetoothManager.h"
// #define HLK_DEBUG //打开日志打印

bool bCancelRegister = false; // 用于取消注册指纹的标志
extern BluetoothManager bluetoothManager; // 蓝牙管理器对象
// 构造函数
Fingerprint::Fingerprint(int rx_pin, int tx_pin)
{
    _rx_pin = rx_pin;
    _tx_pin = tx_pin;
    _buffer_id = 0;
}

// 初始化函数
void Fingerprint::begin(uint32_t baud_rate)
{
    Serial1.begin(baud_rate, SERIAL_8N1, _rx_pin, _tx_pin);
    pinMode(PIN_FINGERPRINT_POWER, OUTPUT);
}

// 打印十六进制数据
void Fingerprint::printHex(uint8_t *data, uint8_t len)
{
    for (uint8_t i = 0; i < len; i++)
    {
        if (data[i] < 0x10)
            Serial.print('0');
        Serial.print(data[i], HEX);
        Serial.print(' ');
    }
    Serial.println();
}

void Fingerprint::setPower(bool on)
{
    if (on)
    {
        digitalWrite(PIN_FINGERPRINT_POWER, HIGH);
        delay(100); // 等待模组上电稳定
        Serial.println("[FP] Fingerprint module powered ON");
    }
    else
    {
        digitalWrite(PIN_FINGERPRINT_POWER, LOW);
        Serial.println("[FP] Fingerprint module powered OFF");
    }
}

// 读取模组基本参数
bool Fingerprint::readInfo()
{
    uint8_t response[32] = {0};
    uint8_t index = 0;
    uint32_t startTime = millis();

    Serial.println("+--------------------+----------------------+");
    Serial.println("|       ZW101 FINGERPRINT SENSOR INFO       |");
    sendCmd12(CMD_READ_SYSPARA);

    // 等待响应包
    while (millis() - startTime < 2000)
    {
        if (Serial1.available())
        {
            response[index++] = Serial1.read();
            if (index >= 28)
                break;
        }
    }

    // 检查确认码
    if (index >= 28 && response[9] == 0x00)
    {
        uint16_t register_cnt = (uint16_t)(response[10] << 8) | response[11];
        uint16_t fp_temp_size = (uint16_t)(response[12] << 8) | response[13];
        uint16_t fp_lib_size = (uint16_t)(response[14] << 8) | response[15];
        uint16_t score_level = (uint16_t)(response[16] << 8) | response[17];
        uint32_t device_addr = (uint32_t)(response[18] << 24) | (response[19] << 16) | (response[20] << 8) | response[21];
        uint16_t data_pack_size = (uint16_t)(response[22] << 8) | response[23];
        if (0 == data_pack_size)
        {
            data_pack_size = 32;
        }
        else if (1 == data_pack_size)
        {
            data_pack_size = 64;
        }
        else if (2 == data_pack_size)
        {
            data_pack_size = 128;
        }
        else if (3 == data_pack_size)
        {
            data_pack_size = 256;
        }
        uint16_t baud_set = (uint16_t)(response[24] << 8) | response[25];

        const int colWidth = 21; // 单元格内容宽度（不含边框）
        char buf[32];

        // 打印表头
        Serial.println("+---------------------+---------------------+");
        Serial.println("|        Name         |        Value        |");
        Serial.println("+---------------------+---------------------+");

        // 打印每一行
        auto printRow = [&](const char* name, const char* value) {
            int nameLen = strlen(name);
            int valueLen = strlen(value);
            int namePad = colWidth - nameLen;
            int valuePad = colWidth - valueLen;
            int namePadLeft = namePad / 2, namePadRight = namePad - namePadLeft;
            int valuePadLeft = valuePad / 2, valuePadRight = valuePad - valuePadLeft;
            Serial.print("|");
            for (int i = 0; i < namePadLeft; ++i) Serial.print(" ");
            Serial.print(name);
            for (int i = 0; i < namePadRight; ++i) Serial.print(" ");
            Serial.print("|");
            for (int i = 0; i < valuePadLeft; ++i) Serial.print(" ");
            Serial.print(value);
            for (int i = 0; i < valuePadRight; ++i) Serial.print(" ");
            Serial.println("|");
        };

        // 逐行输出
        snprintf(buf, sizeof(buf), "%u", register_cnt);
        printRow("REGISTER TIMES", buf);
        snprintf(buf, sizeof(buf), "0x%X", fp_temp_size);
        printRow("TEMPLATE SIZE", buf);
        snprintf(buf, sizeof(buf), "%u", fp_lib_size);
        printRow("LIBRARY SIZE", buf);
        snprintf(buf, sizeof(buf), "%u", score_level);
        printRow("SCORE LEVEL", buf);
        snprintf(buf, sizeof(buf), "0x%lX", device_addr);
        printRow("DEVICE ADDRESS", buf);
        snprintf(buf, sizeof(buf), "%u", data_pack_size);
        printRow("DATA PACK SIZE", buf);
        snprintf(buf, sizeof(buf), "%u", baud_set * 9600);
        printRow("BAUD RATE", buf);

        Serial.println("+---------------------+---------------------+");
        return true; // 成功
    }
    else
    {
        return false; // 失败
    }
}

// 注册指纹
bool Fingerprint::registerFingerprint(int template_id)
{
    _buffer_id = 1;
    while (_buffer_id <= 5)
    {
        Serial.println("Please touch the sensor to register fingerprint...");
        //发送放上手指消息
        bluetoothManager.sendMessage(MSG_PUT_FINGER, &MSG_CMD_EXECUTE, 1);
        while (digitalRead(PIN_FINGERPRINT_TOUCH) == LOW)
        {
            // 等待手指接触传感器
            if (bCancelRegister)
            {
                Serial.println("Fingerprint registration cancelled.");
                return false; // 取消注册
            }
            delay(100);
        }

        // 步骤1：获取图像
        sendCmd12(CMD_GET_IMAGE);
        // 等待指纹模组响应
        if (receiveResponse())
        {
            Serial.println("Get Image OK!");
        }
        else
        {
            Serial.println("Get Image Failed!");
            bluetoothManager.sendMessage(MSG_PUT_FINGER, &MSG_CMD_FAILURE, 1);
            delay(1000);
            continue;
        }
        // 步骤2：生成特征
        sendCmd13(CMD_GEN_CHAR, _buffer_id);
        if (receiveResponse())
        {
            Serial.println("CMD_GEN_CHAR OK!");
            _buffer_id++;
        }
        else
        {
            Serial.println("CMD_GEN_CHAR Failed!");
            bluetoothManager.sendMessage(MSG_PUT_FINGER, &MSG_CMD_FAILURE, 1);
            continue;
        }

        // 发送成功消息
        bluetoothManager.sendMessage(MSG_PUT_FINGER, &MSG_CMD_SUCCESS, 1);
       
        Serial.println("Please remove your finger from the sensor...");
        // 发送要求离开手指消息
        bluetoothManager.sendMessage(MSG_REMOVE_FINGER, &MSG_CMD_SUCCESS, 1);
        while (digitalRead(PIN_FINGERPRINT_TOUCH) == HIGH)
        {
            if (bCancelRegister)
            {
                Serial.println("Fingerprint registration cancelled.");
                return false; // 取消注册
            }
            // 等待手指离开传感器
            delay(100);
        }
    }

    // 步骤3：合并特征
    sendCmd12(CMD_REG_MODEL);
    if (receiveResponse())
    {
        Serial.println("CMD_REG_MODEL OK!");
    }
    else
    {
        Serial.println("CMD_REG_MODEL Failed!");
        return 0;
    }

    // 步骤4：存储模板
    _buffer_id = 1;
    sendCmd15(CMD_STORE_CHAR, _buffer_id, template_id);
    if (receiveResponse())
    {
        Serial.print("CMD_STORE_CHAR OK! Template ID: ");
        Serial.println(template_id);
    }
    else
    {
        return 0;
    }

    return 1;
}

// 搜索指纹
bool Fingerprint::searchFingerprint()
{
    int serch_cnt = 0;
    _buffer_id = 1;
    while (serch_cnt <= 5)
    {
        // 步骤1：获取图像
        sendCmd12(CMD_GET_IMAGE);

        // 等待指纹模组响应
        if (receiveResponse())
        {
            Serial.println("Get Image OK!");
        }
        else
        {
            Serial.println("Get Image Failed!");
            serch_cnt++;
            delay(1000);
            continue;
        }
        // 步骤2：生成特征
        sendCmd13(CMD_GEN_CHAR, _buffer_id);
        if (receiveResponse())
        {
            Serial.println("CMD_GEN_CHAR OK!");
            break;
        }
        else
        {
            Serial.println("CMD_GEN_CHAR Failed!");
            serch_cnt++;
            delay(500);
            continue;
        }
    }
    // 步骤3：搜索指纹
    _buffer_id = 1;
    sendCmd17(CMD_SEARCH, _buffer_id, 1, 1);
    if (receiveResponse())
    {
        Serial.println("CMD_SEARCH OK!");
        return 1;
    }
    Serial.println("CMD_SEARCH Failed!");
    return 0;
}

bool Fingerprint::autoIdentifyFingerprint()
{
    int search_cnt = 0;
    _buffer_id = 1;
    int startTime = millis();

    // 发送命令
    sendCmd17(CMD_AUTO_IDENTIFY, 0, 0xFFFF, 0);
    // 等待指纹模组响应
    if (receiveResponse())
    {
        Serial.print("指令合法性检测 OK!");
        Serial.println(millis() - startTime);
        startTime = millis();
        if (receiveResponse())
        {
            Serial.print("采图结果 OK!");
            if (receiveResponse())
            {
                Serial.print("搜索结果 OK!");
                return true;
            }
        }
    }
}

// 删除指定指纹
bool Fingerprint::deleteFingerprint(uint16_t id)
{
    // 发送删除指令
    sendCmd16(CMD_DELETE_CHAR, id, 1);

    // 等待响应包
    if (receiveResponse())
    {
        Serial.print("Deleted fingerprint with ID: ");
        Serial.println(id);
        return true; // 成功
    }
    else
    {
        Serial.println("Failed to delete fingerprint");
        return false; // 失败
    }
}

// 清空指纹库
bool Fingerprint::clearAllLib()
{
    sendCmd12(CMD_CLEAR_LIB);
    if (receiveResponse())
    {
        return 1;
    }
    return 0;
}

bool Fingerprint::setLEDAutoManual(int autoMode)
{
    // 发送呼吸灯自动手动切换指令
    sendCmd13(CMD_LED_AUTO_MANUAL, autoMode);
    if (receiveResponse())
    {
        Serial.print("LED Auto/Manual Mode set to: ");
        Serial.println(autoMode == 0xFF ? "Auto" : "Manual");
        return true; // 成功
    }
    else
    {
        Serial.println("Failed to set LED Auto/Manual Mode");
        return false; // 失败
    }
}

bool Fingerprint::setLEDCmd(uint8_t code, uint8_t startColor, uint8_t endColor, uint8_t loopCount)
{
    // 发送呼吸灯指令
    sendCmd16(CMD_LED_CM, code, startColor, endColor, loopCount);
    if (receiveResponse())
    {
        Serial.print("LED Command set: ");
        Serial.print("Code: ");
        Serial.print(code, HEX);
        Serial.print(", Start Color: ");
        Serial.print(startColor, HEX);
        Serial.print(", End Color: ");
        Serial.print(endColor, HEX);
        Serial.print(", Loop Count: ");
        Serial.println(loopCount);
        return true; // 成功
    }
    else
    {
        Serial.println("Failed to set LED Command");
        return false; // 失败
    }
}

bool Fingerprint::setLEDCmd(uint8_t code, uint8_t startColor, uint8_t endColorOrdutyCicle, uint8_t loopCount, uint8_t time)
{
    // 发送呼吸灯指令
    sendCmd17(CMD_LED_CM, code, startColor, endColorOrdutyCicle, loopCount, time);
    if (receiveResponse())
    {
        Serial.print("LED Command set: ");
        Serial.print("Code: ");
        Serial.print(code, HEX);
        Serial.print(", Start Color: ");
        Serial.print(startColor, HEX);
        Serial.print(", End Color Or Duty Cycle: ");
        Serial.print(endColorOrdutyCicle, HEX);
        Serial.print(", Loop Count: ");
        Serial.println(loopCount);
        Serial.print(", Time: ");
        Serial.println(time);
        return true; // 成功
    }
    else
    {
        Serial.println("Failed to set LED Command");
        return false; // 失败
    }
}

// 读取有效模板数量
int Fingerprint::readValidTempleteNum()
{
    sendCmd12(CMD_VALID_TEMPLATE_NUM);
    int templateNum = 0;
    if (receiveResponse(templateNum))
    {
        return templateNum;
    }
    return 0;
}

// 读取索引表
bool Fingerprint::readIndexTable(uint8_t *indexTable)
{
    // 初始化索引表
    memset(indexTable, 0, 32);
    
    sendCmd13(CMD_READ_INDEX, 0);
    if (receiveIndexTable(indexTable))
    {
        Serial.println("Index Table read OK");
        Serial.print("Index Table: ");
        for (int i = 0; i < 32; i++)
        {
            Serial.print("[" + String(i) + "]:");
            for (int bit = 0; bit < 8; bit++) {
                Serial.print((indexTable[i] & (1 << bit)) ? "1" : "0");
            }
            Serial.print(" ");
        }
        Serial.println();
        return true; // 成功
    }
    else
    {
        Serial.println("Failed to read index table");
        // 确保失败时索引表被清零
        memset(indexTable, 0, 32);
        return false; // 失败
    }
}

// 休眠
bool Fingerprint::sleepFingerprint()
{
    sendCmd12(CMD_SLEEP); // 发送休眠指令
    if (receiveResponse())
    {
        return true; // 成功
    }
    else
    {
        return false; // 失败
    }
}

// 发送指令包
void Fingerprint::sendCmd12(uint8_t cmd)
{
    uint8_t packet[12];
    uint16_t length = 3;
    uint16_t checksum = 1 + length + cmd;

    // 构建指令包
    packet[0] = HEADER_HIGH;
    packet[1] = HEADER_LOW;
    packet[2] = (DEVICE_ADDRESS >> 24) & 0xFF;
    packet[3] = (DEVICE_ADDRESS >> 16) & 0xFF;
    packet[4] = (DEVICE_ADDRESS >> 8) & 0xFF;
    packet[5] = DEVICE_ADDRESS & 0xFF;
    packet[6] = 0x01;                 // 包标识：命令包
    packet[7] = (length >> 8) & 0xFF; // 包长度高字节
    packet[8] = length & 0xFF;        // 包长度低字节
    packet[9] = cmd;
    packet[10] = (checksum >> 8) & 0xFF;
    packet[11] = checksum & 0xFF;

#if defined(HLK_DEBUG)
    Serial.println("send1:");
    printHex(packet, (12));
#endif

    // 发送指令包
    for (int i = 0; i < (12); i++)
    {
        Serial1.write(packet[i]);
    }
}

// 发送指令包
void Fingerprint::sendCmd13(uint8_t cmd, uint8_t param1)
{
    uint8_t packet[13];
    uint16_t length = 4;
    uint16_t checksum = 1 + length + cmd + param1;

    // 构建指令包
    packet[0] = HEADER_HIGH;
    packet[1] = HEADER_LOW;
    packet[2] = (DEVICE_ADDRESS >> 24) & 0xFF;
    packet[3] = (DEVICE_ADDRESS >> 16) & 0xFF;
    packet[4] = (DEVICE_ADDRESS >> 8) & 0xFF;
    packet[5] = DEVICE_ADDRESS & 0xFF;
    packet[6] = 0x01;                 // 包标识：命令包
    packet[7] = (length >> 8) & 0xFF; // 包长度高字节
    packet[8] = length & 0xFF;        // 包长度低字节
    packet[9] = cmd;
    packet[10] = param1;
    packet[11] = (checksum >> 8) & 0xFF;
    packet[12] = checksum & 0xFF;

#if defined(HLK_DEBUG)
    Serial.println("send2:");
    printHex(packet, (13));
#endif

    // 发送指令包
    for (int i = 0; i < (13); i++)
    {
        Serial1.write(packet[i]);
    }
}

// 发送指令包
void Fingerprint::sendCmd15(uint8_t cmd, uint8_t param1, uint16_t param2)
{
    uint8_t packet[15];
    uint16_t length = 6;
    uint16_t checksum = 1 + length + cmd + param1 + (param2 >> 8) + (param2 & 0xFF);

    // 构建指令包
    packet[0] = HEADER_HIGH;
    packet[1] = HEADER_LOW;
    packet[2] = (DEVICE_ADDRESS >> 24) & 0xFF;
    packet[3] = (DEVICE_ADDRESS >> 16) & 0xFF;
    packet[4] = (DEVICE_ADDRESS >> 8) & 0xFF;
    packet[5] = DEVICE_ADDRESS & 0xFF;
    packet[6] = 0x01;                 // 包标识：命令包
    packet[7] = (length >> 8) & 0xFF; // 包长度高字节
    packet[8] = length & 0xFF;        // 包长度低字节
    packet[9] = cmd;
    packet[10] = param1;
    packet[11] = (param2 >> 8) & 0xFF;
    packet[12] = param2 & 0xFF;
    packet[13] = (checksum >> 8) & 0xFF;
    packet[14] = checksum & 0xFF;

#if defined(HLK_DEBUG)
    Serial.println("send:");
    printHex(packet, (15));
#endif

    // 发送指令包
    for (int i = 0; i < (15); i++)
    {
        Serial1.write(packet[i]);
    }
}

// 发送指令包
void Fingerprint::sendCmd16(uint8_t cmd, uint8_t param1, uint16_t param2)
{
    uint8_t packet[16];
    uint16_t length = 7;
    uint16_t checksum = 1 + length + cmd + (param1 >> 8) + (param1 & 0xFF) + (param2 >> 8) + (param2 & 0xFF);

    // 构建指令包
    packet[0] = HEADER_HIGH;
    packet[1] = HEADER_LOW;
    packet[2] = (DEVICE_ADDRESS >> 24) & 0xFF;
    packet[3] = (DEVICE_ADDRESS >> 16) & 0xFF;
    packet[4] = (DEVICE_ADDRESS >> 8) & 0xFF;
    packet[5] = DEVICE_ADDRESS & 0xFF;
    packet[6] = 0x01;                 // 包标识：命令包
    packet[7] = (length >> 8) & 0xFF; // 包长度高字节
    packet[8] = length & 0xFF;        // 包长度低字节
    packet[9] = cmd;
    packet[10] = (param1 >> 8) & 0xFF;
    packet[11] = param1 & 0xFF;
    packet[12] = (param2 >> 8) & 0xFF;
    packet[13] = param2 & 0xFF;
    packet[14] = (checksum >> 8) & 0xFF;
    packet[15] = checksum & 0xFF;

#if defined(HLK_DEBUG)
    Serial.println("send:");
    printHex(packet, (16));
#endif

    // 发送指令包
    for (int i = 0; i < (16); i++)
    {
        Serial1.write(packet[i]);
    }
}

// 发送指令包
void Fingerprint::sendCmd16(uint8_t cmd, uint8_t param1, uint16_t param2, uint16_t param3, uint16_t param4)
{
    uint8_t packet[16];
    uint16_t length = 7;
    uint16_t checksum = 1 + length + cmd + param1 + param2 + param3 + param4;

    // 构建指令包
    packet[0] = HEADER_HIGH;
    packet[1] = HEADER_LOW;
    packet[2] = (DEVICE_ADDRESS >> 24) & 0xFF;
    packet[3] = (DEVICE_ADDRESS >> 16) & 0xFF;
    packet[4] = (DEVICE_ADDRESS >> 8) & 0xFF;
    packet[5] = DEVICE_ADDRESS & 0xFF;
    packet[6] = 0x01;                 // 包标识：命令包
    packet[7] = (length >> 8) & 0xFF; // 包长度高字节
    packet[8] = length & 0xFF;        // 包长度低字节
    packet[9] = cmd;
    packet[10] = param1;
    packet[11] = param2;
    packet[12] = param3;
    packet[13] = param4;
    packet[14] = (checksum >> 8) & 0xFF;
    packet[15] = checksum & 0xFF;

#if defined(HLK_DEBUG)
    Serial.println("send:");
    printHex(packet, (16));
#endif

    // 发送指令包
    for (int i = 0; i < (16); i++)
    {
        Serial1.write(packet[i]);
    }
}

// 发送指令包
void Fingerprint::sendCmd17(uint8_t cmd, uint8_t param1, uint16_t param2, uint16_t param3)
{
    uint8_t packet[17];
    uint16_t length = 8;
    uint16_t checksum = 1 + length + cmd + param1 + (param2 >> 8) + (param2 & 0xFF) + (param3 >> 8) + (param3 & 0xFF);

    // 构建指令包
    packet[0] = HEADER_HIGH;
    packet[1] = HEADER_LOW;
    packet[2] = (DEVICE_ADDRESS >> 24) & 0xFF;
    packet[3] = (DEVICE_ADDRESS >> 16) & 0xFF;
    packet[4] = (DEVICE_ADDRESS >> 8) & 0xFF;
    packet[5] = DEVICE_ADDRESS & 0xFF;
    packet[6] = 0x01;                 // 包标识：命令包
    packet[7] = (length >> 8) & 0xFF; // 包长度高字节
    packet[8] = length & 0xFF;        // 包长度低字节
    packet[9] = cmd;
    packet[10] = param1;
    packet[11] = (param2 >> 8) & 0xFF;
    packet[12] = param2 & 0xFF;
    packet[13] = (param3 >> 8) & 0xFF;
    packet[14] = param3 & 0xFF;
    packet[15] = (checksum >> 8) & 0xFF;
    packet[16] = checksum & 0xFF;

#if defined(HLK_DEBUG)
    Serial.println("send:");
    printHex(packet, (17));
#endif

    // 发送指令包
    for (int i = 0; i < (17); i++)
    {
        Serial1.write(packet[i]);
    }
}

void Fingerprint::sendCmd17(uint8_t cmd, uint8_t param1, uint8_t param2, uint8_t param3, uint8_t param4, uint8_t param5)
{
    uint8_t packet[17];
    uint16_t length = 8;
    uint16_t checksum = 1 + length + cmd + param1 + param2 + param3 + param4 + param5;

    // 构建指令包
    packet[0] = HEADER_HIGH;
    packet[1] = HEADER_LOW;
    packet[2] = (DEVICE_ADDRESS >> 24) & 0xFF;
    packet[3] = (DEVICE_ADDRESS >> 16) & 0xFF;
    packet[4] = (DEVICE_ADDRESS >> 8) & 0xFF;
    packet[5] = DEVICE_ADDRESS & 0xFF;
    packet[6] = 0x01;                 // 包标识：命令包
    packet[7] = (length >> 8) & 0xFF; // 包长度高字节
    packet[8] = length & 0xFF;        // 包长度低字节
    packet[9] = cmd;
    packet[10] = param1;
    packet[11] = param2;
    packet[12] = param3;
    packet[13] = param4;
    packet[14] = param5;
    packet[15] = (checksum >> 8) & 0xFF;
    packet[16] = checksum & 0xFF;

#if defined(HLK_DEBUG)
    Serial.println("send:");
    printHex(packet, (17));
#endif

    // 发送指令包
    for (int i = 0; i < (17); i++)
    {
        Serial1.write(packet[i]);
    }
}

// 接收响应包
bool Fingerprint::receiveResponse()
{
    uint8_t response[50] = {0};
    uint8_t index = 0;
    uint32_t startTime = millis();
    uint32_t lastDataTime = startTime;

    // 等待响应包
    while (millis() - startTime < 500)
    { // 保留超时机制，但可以提前退出
        if (Serial1.available())
        {
            response[index++] = Serial1.read();
            lastDataTime = millis(); // 更新最后一次接收数据的时间

            // 检查是否已接收到完整的响应包
            // 通常响应包至少有12字节，索引到11，且第9字节是确认码
            if (index >= 12)
            {
                // 检查包头
                if (response[0] == HEADER_HIGH && response[1] == HEADER_LOW)
                {
                    // 获取包长度
                    uint16_t packageLength = (response[7] << 8) | response[8];
                    // 计算完整包应有的长度：包头(2) + 设备地址(4) + 包标识(1) + 包长度(2) + 校验和(2)
                    uint16_t totalLength = 2 + 4 + 1 + packageLength + 2;

                    // 如果已接收到完整包，提前退出
                    if (index >= totalLength)
                    {
                        break;
                    }
                }
            }
        }
        else
        {
            // 如果一段时间内没有新数据（例如20ms），且已经接收了一些数据，可能表示数据已传输完毕
            if (index > 0 && (millis() - lastDataTime > 200))
            {
                break;
            }
        }
    }

    // 打印响应包
#if defined(HLK_DEBUG)
    printResponse(response, index);
#endif

    // 检查确认码
    if (index >= 12 && response[9] == 0x00)
    {
        return true; // 成功
    }
    else
    {
        return false; // 失败
    }
}

bool Fingerprint::waitStartSignal()
{
    uint32_t startTime = millis();

    // 等待响应包，最多等待500毫秒
    while (millis() - startTime < 500)
    { // 保留超时机制，但可以提前退出
        if (Serial1.available())
        {
            if(Serial1.read() == 0x55) // 检测到开始信号
            {
                Serial.println("[FP] Start signal received.");
                return true; // 成功接收到开始信号
            }
        }
        delay(10); // 等待一段时间再检查
    }
    return false; // 超时未接收到开始信号
}

// 接收响应包
bool Fingerprint::receiveResponse(int &data)
{
    uint8_t response[50] = {0};
    uint8_t index = 0;
    uint32_t startTime = millis();
    uint32_t lastDataTime = startTime;

    // 等待响应包
    while (millis() - startTime < 500)
    { // 保留超时机制，但可以提前退出
        if (Serial1.available())
        {
            response[index++] = Serial1.read();
            lastDataTime = millis(); // 更新最后一次接收数据的时间

            // 检查是否已接收到完整的响应包
            // 通常响应包至少有12字节，且第9字节是确认码
            if (index >= 12)
            {
                // 检查包头
                if (response[0] == HEADER_HIGH && response[1] == HEADER_LOW)
                {
                    // 获取包长度
                    uint16_t packageLength = (response[7] << 8) | response[8];
                    // 计算完整包应有的长度：包头(2) + 设备地址(4) + 包标识(1) + 包内容(packageLength) + 校验和(2)
                    uint16_t totalLength = 2 + 4 + 1 + packageLength + 2;

                    // 如果已接收到完整包，提前退出
                    if (index >= totalLength)
                    {
                        break;
                    }
                }
            }
        }
        else
        {
            // 如果一段时间内没有新数据（例如200ms），且已经接收了一些数据，可能表示数据已传输完毕
            if (index > 0 && (millis() - lastDataTime > 200))
            {
                break;
            }
        }
    }

    // 打印响应包
#if defined(HLK_DEBUG)
    printResponse(response, index);
#endif

    data = (response[10] << 8) | response[11]; // 获取数据包中的数据
    // 检查确认码
    if (index >= 12 && response[9] == 0x00)
    {
        return true; // 成功
    }
    else
    {
        return false; // 失败
    }
}


// 接收响应包
bool Fingerprint::receiveIndexTable(uint8_t* data)
{
    uint8_t response[45] = {0};
    uint8_t index = 0;
    uint32_t startTime = millis();
    uint32_t lastDataTime = startTime;

    // 等待响应包
    while (millis() - startTime < 1000)
    { // 保留超时机制，但可以提前退出
        if (Serial1.available())
        {
            response[index++] = Serial1.read();
            lastDataTime = millis(); // 更新最后一次接收数据的时间

            // 检查是否已接收到完整的响应包
            // 通常响应包至少有12字节，且第9字节是确认码
            if (index >= 12)
            {
                // 检查包头
                if (response[0] == HEADER_HIGH && response[1] == HEADER_LOW)
                {
                    // 获取包长度
                    uint16_t packageLength = (response[7] << 8) | response[8];
                    // 计算完整包应有的长度：包头(2) + 设备地址(4) + 包标识(1) + 包长度(2) + 包内容(packageLength) 
                    uint16_t totalLength = 2 + 4 + 1 + 2 + packageLength;

                    // 如果已接收到完整包，提前退出
                    if (index >= totalLength)
                    {
                        break;
                    }
                }
            }
        }
        else
        {
            // 如果一段时间内没有新数据（例如200ms），且已经接收了一些数据，可能表示数据已传输完毕
            if (index > 0 && (millis() - lastDataTime > 200))
            {
                break;
            }
        }
    }

    // 检查是否接收到了足够的数据
    if (index < 12) {
        Serial.printf("[FP] receiveIndexTable: Insufficient data received, only %d bytes\n", index);
        return false;
    }

    // 检查包头
    if (response[0] != HEADER_HIGH || response[1] != HEADER_LOW) {
        Serial.printf("[FP] receiveIndexTable: Invalid header %02X %02X\n", response[0], response[1]);
        return false;
    }

    // 打印响应包
#if defined(HLK_DEBUG)
    printResponse(response, index);
#endif
    // 从索引10开始复制32字节到data数组
    memcpy(data, &response[10], 32);
    // 检查确认码
    if (response[9] == 0x00)
    {
        Serial.println("[FP] receiveIndexTable: Success");
        return true; // 成功
    }
    else
    {
        Serial.printf("[FP] receiveIndexTable: Error response code %02X\n", response[9]);
        return false; // 失败
    }
}
// 打印响应包
void Fingerprint::printResponse(uint8_t *response, uint8_t length)
{
    Serial.print("Response:");
    for (int i = 0; i < length; i++)
    {
        if (response[i] < 0x10)
            Serial.print('0');
        Serial.print(response[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
}