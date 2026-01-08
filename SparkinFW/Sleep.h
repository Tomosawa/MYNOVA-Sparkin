/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
#ifndef SLEEP_H
#define SLEEP_H

#include <Arduino.h>
// 进入轻度睡眠模式
int enterLightSleep();

// 配置GPIO唤醒源
void configureWakeupSources();

#endif // SLEEP_H
