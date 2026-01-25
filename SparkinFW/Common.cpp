/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
#include "Common.h"

 // 中断标志变量
volatile bool touchTriggered = false;

// 触摸传感器中断处理函数
void IRAM_ATTR handleTouchInterrupt() {
  touchTriggered = true; // 设置中断标志
}

// 事件组句柄
EventGroupHandle_t event_group;
// 初始化事件组
void init_event_group() {
    event_group = xEventGroupCreate();
}

// bPairMode已移除，改用 bluetoothManager.isPairingMode() 动态判断
float batteryPercentage = 100.0; // 电池电量百分比