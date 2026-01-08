/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
#include "Sleep.h"
#include <esp_sleep.h>
#include <esp_wifi.h>
#include "IOPin.h"

void configureWakeupSources() {
    // 确保引脚配置为输入模式
    gpio_pulldown_en((gpio_num_t)PIN_FINGERPRINT_TOUCH);
    
    gpio_wakeup_enable((gpio_num_t)PIN_FINGERPRINT_TOUCH, GPIO_INTR_HIGH_LEVEL);
    gpio_wakeup_enable((gpio_num_t)PIN_PAIR_BUTTON, GPIO_INTR_LOW_LEVEL);
    // 配置GPIO唤醒源
    esp_sleep_enable_gpio_wakeup();
}

int enterLightSleep() {
    Serial.println("[SLEEP]ESP32进入轻度睡眠模式");
      // 确保唤醒源已正确配置
    configureWakeupSources();
    // 等待串口输出完成
    Serial.flush();
    
    // 进入轻度睡眠
    esp_light_sleep_start();

    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    int wakeup_pin = -1;  // 初始化唤醒引脚变量
    switch(wakeup_reason) {
        case ESP_SLEEP_WAKEUP_GPIO:{
            Serial.println("[SLEEP]GPIO唤醒");
            // 检查具体GPIO
            if (digitalRead(PIN_PAIR_BUTTON) == LOW) {
                Serial.println("[SLEEP]由配对按键引脚唤醒");
                wakeup_pin = PIN_PAIR_BUTTON;
            }
            else {
                Serial.println("[SLEEP]由指纹触摸引脚唤醒");
                wakeup_pin = PIN_FINGERPRINT_TOUCH;
            }
            break;
        }
        case ESP_SLEEP_WAKEUP_UART:
            Serial.println("[SLEEP]UART唤醒");
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            Serial.println("[SLEEP]定时器唤醒");
            break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            Serial.println("[SLEEP]触摸唤醒");
            break;
        case ESP_SLEEP_WAKEUP_ULP:
            Serial.println("[SLEEP]ULP唤醒");
            break;
        case ESP_SLEEP_WAKEUP_WIFI:
            Serial.println("[SLEEP]WiFi唤醒");
            break;
        case ESP_SLEEP_WAKEUP_UNDEFINED:
            Serial.println("[SLEEP]未定义唤醒");
            break;
        default:
            Serial.printf("[SLEEP]其他唤醒源: %d\n", wakeup_reason);
            break;
    }
    
    // 醒来后的处理
    gpio_wakeup_disable((gpio_num_t)PIN_PAIR_BUTTON);
    
    Serial.println("[SLEEP]设备已唤醒");
    return wakeup_pin;
}
