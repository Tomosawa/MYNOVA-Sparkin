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

bool bSleepMode = false; // 是否进入睡眠模式
bool bEnableSleep = true; // 是否启用自动休眠
bool bPairMode = false; // 是否处于配对模式
float batteryPercentage = 100.0; // 电池电量百分比