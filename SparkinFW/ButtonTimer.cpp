/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
#include "ButtonTimer.h"
#include "Common.h"
#include "ButtonHandle.h"
#include "Fingerprint.h"

extern TaskHandle_t g_buttonTaskHandle; // 在ButtonHandler.cpp里定义
extern Fingerprint fingerprint; // 在Fingerprint.cpp里定义

ButtonTimer::ButtonTimer()
    : _pin(0),
      _mux(portMUX_INITIALIZER_UNLOCKED),
      _currentState(false), _lastSteadyState(false), _lastFlickerableState(false),
      _lastDebounceTime(0), _pressStartTime(0), _lastEvent(Event::NONE), _debounceMs(50) {
}

void ButtonTimer::begin(int pin) {
    _pin = pin;
   
    _currentState = digitalRead(_pin) == LOW;
    _lastFlickerableState = _currentState;
    _lastDebounceTime = millis();
    // 统一从"释放"状态开始，让首次稳定采样能触发PRESS（尤其是按住按键唤醒的场景）
    _lastSteadyState = false;
    // 如果当前按键已经按下（唤醒场景），将按下时间设置为当前时间，以便后续长按检测
    _pressStartTime = _currentState ? millis() : 0;
    _lastEvent = Event::NONE;
    
    // 调试信息：打印初始状态
    Serial.printf("[ButtonTimer] 初始化 - 当前状态: %d (0=释放, 1=按下)\n", _currentState);
}

void ButtonTimer::end() {
    // 移除硬件定时器清理
}

bool ButtonTimer::isPressed() const {
    return _currentState;
}

bool ButtonTimer::isIdle() const {
    // 空闲条件：
    // 1. 逻辑状态为释放 (_currentState == false)
    // 2. 物理状态为释放 (digitalRead == HIGH)
    // 3. 距离上次消抖时间超过 debounceMs + 50ms缓冲 (确保完全稳定)
    return !_currentState && (digitalRead(_pin) == HIGH) && ((millis() - _lastDebounceTime) > (_debounceMs + 50));
}

void ButtonTimer::poll() {
    // 读取当前引脚状态
    bool reading = digitalRead(_pin) == LOW;

    unsigned long now = millis();

    // 消抖处理
    if (reading != _lastFlickerableState) {
        _lastDebounceTime = now;
        _lastFlickerableState = reading;
    }

    if ((now - _lastDebounceTime) > _debounceMs) {
        // 状态稳定，可以更新
        if (reading != _lastSteadyState) {
            _lastSteadyState = reading;
            _currentState = reading;

            if (reading) {
                // 按键被按下
                _pressStartTime = now;
                Serial.printf("[ButtonTimer] 检测到按键按下，开始计时\n");
                handleButtonEvent(Event::PRESS);
                _lastEvent = Event::PRESS;
            } else {
                // 按键被释放
                Serial.printf("[ButtonTimer] 检测到按键释放\n");
                handleButtonEvent(Event::RELEASE);
                _lastEvent = Event::RELEASE;
            }
        } else if (reading) {
            // 按键保持按下状态，检查长按时间
            unsigned long pressDuration = now - _pressStartTime;

            if (pressDuration >= 10000 && _lastEvent != Event::LONG_PRESS_10S) {
                Serial.printf("[ButtonTimer] 检测到10秒长按\n");
                handleButtonEvent(Event::LONG_PRESS_10S);
                _lastEvent = Event::LONG_PRESS_10S;
            } else if (pressDuration >= 3000 && _lastEvent != Event::LONG_PRESS_3S && _lastEvent != Event::LONG_PRESS_10S) {
                Serial.printf("[ButtonTimer] 检测到3秒长按\n");
                handleButtonEvent(Event::LONG_PRESS_3S);
                _lastEvent = Event::LONG_PRESS_3S;
            }
        }
    }
}

void ButtonTimer::handleButtonEvent(ButtonTimer::Event event) {
    switch (event) {
        case ButtonTimer::Event::PRESS:
            Serial.println("按键按下");
            if (g_buttonTaskHandle)
            {
                xTaskNotify(g_buttonTaskHandle, BUTTON_NOTIFY_PRESS, eSetBits);
            }
            break;
        case ButtonTimer::Event::RELEASE:
            Serial.println("按键释放");
            if (g_buttonTaskHandle)
            {
                xTaskNotify(g_buttonTaskHandle, BUTTON_NOTIFY_RELEASE, eSetBits);
            }
            if(_lastEvent == Event::LONG_PRESS_3S)
            {
                // 如果之前是释放状态，发送通知
                if (g_buttonTaskHandle)
                {
                    xTaskNotify(g_buttonTaskHandle, BUTTON_RELEASE_3S, eSetBits);
                }
            }
            if(_lastEvent == Event::LONG_PRESS_10S)
            {
                // 如果之前是释放状态，发送通知
                if (g_buttonTaskHandle)
                {
                    xTaskNotify(g_buttonTaskHandle, BUTTON_RELEASE_10S, eSetBits);
                }
            }
            break;
        case ButtonTimer::Event::LONG_PRESS_3S:
            Serial.println("检测到3秒长按");
            if (g_buttonTaskHandle)
            {   
                xTaskNotify(g_buttonTaskHandle, BUTTON_NOTIFY_3S, eSetBits);
            }
            break;
        case ButtonTimer::Event::LONG_PRESS_10S:
            Serial.println("检测到10秒长按");
            if (g_buttonTaskHandle)
            {
                xTaskNotify(g_buttonTaskHandle, BUTTON_NOTIFY_10S, eSetBits);
            }
            break;
        case ButtonTimer::Event::NONE:
        default:
            break;
    }
}