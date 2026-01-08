/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
#ifndef BLUETOOTH_HANDLE_H
#define BLUETOOTH_HANDLE_H
#include <Arduino.h>

#define MAX_DATA_LENGTH 300  // 从电脑端最大接收数据长度
// 任务处理函数的参数结构
struct TaskParameters {
    uint8_t msgType;
    uint8_t data[MAX_DATA_LENGTH];
    size_t length;
};

#ifdef __cplusplus
extern "C" {
#endif

void initBluetoothMessageQueue();

void bluetoothMessageQueueTask(void* pvParameters);

void handleBluetoothMessage(uint8_t msgType, uint8_t* data, size_t length);

void bluetoothMessageTask(TaskParameters* params);

#ifdef __cplusplus
}
#endif

#endif