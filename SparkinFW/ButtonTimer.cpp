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
      _timer(nullptr), _mux(portMUX_INITIALIZER_UNLOCKED),
      _currentState(false), _lastSteadyState(false), _lastFlickerableState(false),
      _lastDebounceTime(0), _pressStartTime(0), _lastEvent(Event::NONE), _debounceMs(50) {
}

void ButtonTimer::begin(int pin) {
    _pin = pin;
    // 配置按键引脚
    pinMode(_pin, INPUT_PULLUP);
    _currentState = digitalRead(_pin) == LOW;
    _lastSteadyState = _currentState;
    _lastFlickerableState = _currentState;
    _lastDebounceTime = millis();
    
    // 配置硬件定时器
    _timer = timerBegin(10000); 
    timerAttachInterruptArg(_timer, &ButtonTimer::_onTimer, this);
    timerWrite(_timer, 0);// 重置计数器
    timerAlarm(_timer, 2000, true, 0); // 设置定时器每200ms触发一次
    if (!_timer) {
        Serial.println("定时器初始化失败！");
        return;
    }
}

void ButtonTimer::end() {
    if (_timer) {
        timerStop(_timer);
        timerDetachInterrupt(_timer);
        timerEnd(_timer);
        _timer = nullptr;
    }
}

bool ButtonTimer::isPressed() const {
    return _currentState;
}

void IRAM_ATTR ButtonTimer::_onTimer(void *arg) {
    ButtonTimer *handler = static_cast<ButtonTimer*>(arg);
    // 读取当前引脚状态
    portENTER_CRITICAL_ISR(&handler->_mux);
    bool reading = digitalRead(handler->_pin) == LOW;
    portEXIT_CRITICAL_ISR(&handler->_mux);

    unsigned long now = millis();

    // 消抖处理
    if (reading != handler->_lastFlickerableState) {
        handler->_lastDebounceTime = now;
        handler->_lastFlickerableState = reading;
    }

    if ((now - handler->_lastDebounceTime) > handler->_debounceMs) {
        // 状态稳定，可以更新
        if (reading != handler->_lastSteadyState) {
            handler->_lastSteadyState = reading;

            if (reading) {
                // 按键被按下
                handler->_pressStartTime = now;
                handler->handleButtonEvent(Event::PRESS);
                handler->_lastEvent = Event::PRESS;
            } else {
                // 按键被释放
                handler->handleButtonEvent(Event::RELEASE);
                handler->_lastEvent = Event::RELEASE;
            }
        } else if (reading) {
            // 按键保持按下状态，检查长按时间
            unsigned long pressDuration = now - handler->_pressStartTime;

            if (pressDuration >= 10000 && handler->_lastEvent != Event::LONG_PRESS_10S) {
                handler->handleButtonEvent(Event::LONG_PRESS_10S);
                handler->_lastEvent = Event::LONG_PRESS_10S;
            } else if (pressDuration >= 3000 && handler->_lastEvent != Event::LONG_PRESS_3S && handler->_lastEvent != Event::LONG_PRESS_10S) {
                handler->handleButtonEvent(Event::LONG_PRESS_3S);
                handler->_lastEvent = Event::LONG_PRESS_3S;
            }
        }
    }

}

void ButtonTimer::handleButtonEvent(ButtonTimer::Event event) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    switch (event) {
        case ButtonTimer::Event::PRESS:
            //Serial.println("按键按下");
            bEnableSleep = false; // 按键按下时禁用休眠
            break;
        case ButtonTimer::Event::RELEASE:
            //Serial.println("按键释放");
            bEnableSleep = true; // 按键释放时允许休眠
            if(_lastEvent == Event::LONG_PRESS_3S)
            {
                // 如果之前是释放状态，发送通知
                if (g_buttonTaskHandle)
                {
                    xTaskNotifyFromISR(g_buttonTaskHandle, BUTTON_RELEASE_3S, eSetBits, &xHigherPriorityTaskWoken);
                }
            }
            if(_lastEvent == Event::LONG_PRESS_10S)
            {
                // 如果之前是释放状态，发送通知
                if (g_buttonTaskHandle)
                {
                    xTaskNotifyFromISR(g_buttonTaskHandle, BUTTON_RELEASE_10S, eSetBits, &xHigherPriorityTaskWoken);
                }
            }
            break;
        case ButtonTimer::Event::LONG_PRESS_3S:
            //Serial.println("检测到3秒长按");
            if (g_buttonTaskHandle)
            {   
                xTaskNotifyFromISR(g_buttonTaskHandle, BUTTON_NOTIFY_3S, eSetBits, &xHigherPriorityTaskWoken);
            }
            break;
        case ButtonTimer::Event::LONG_PRESS_10S:
            if (g_buttonTaskHandle)
            {
                xTaskNotifyFromISR(g_buttonTaskHandle, BUTTON_NOTIFY_10S, eSetBits, &xHigherPriorityTaskWoken);
            }
            break;
        case ButtonTimer::Event::NONE:
        default:
            break;
    }
    // 如果需要，强制任务切换
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}