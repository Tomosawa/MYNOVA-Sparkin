/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
#ifndef BATTERY_MANAGER_H
#define BATTERY_MANAGER_H

#include <Arduino.h>

class BatteryManager {
public:
    BatteryManager(int adcPin, int testPin);
    ~BatteryManager();

    void init();
    float readVoltage();
    float calculateBatteryPercent(float voltage);
    void CheckBatteryLow();
private:
    // 内部ADC读取方法
    uint32_t readADC();
private:
    int adcPin;  // ADC引脚
    int testPin; // 测试引脚
};

#endif