/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
#ifndef COMMON_H
#define COMMON_H
#pragma once
#include <Arduino.h>

#define MAX_FINGERPRINT_NUM 50 // 最大的指纹数量
#define INDEX_TABLE_LENGTH  32 // 索引表长度
#define MAX_FINGERNAME_LENGTH 32 //最大的指纹名称长度

#define EVENT_BIT_SCREENLOCK (1 << 0)  // 屏幕锁定事件位
#define EVENT_BIT_BLE_CONNECTED (1 << 1) // 蓝牙连接事件位
#define EVENT_BIT_BLE_DISCONNECTED (1 << 2) // 蓝牙断开事件位
#define EVENT_BIT_BLE_NOTIFY (1 << 3) // 蓝牙订阅成功可通讯了
#define EVENT_BIT_BLE_SEARCH (1 << 4) // 蓝牙指纹解锁事件位
#define EVENT_BIT_BLE_SLEEP_ENABLE (1 << 5) // 蓝牙休眠事件位
#ifdef __cplusplus
extern "C" {
#endif

extern EventGroupHandle_t event_group;
extern volatile bool touchTriggered;
// bPairMode已移除，改用 bluetoothManager.isPairingMode() 动态判断
extern float batteryPercentage; // 电池电量百分比

void IRAM_ATTR handleTouchInterrupt();

void init_event_group();

#ifdef __cplusplus
}
#endif

#endif // COMMON_H