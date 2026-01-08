/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
#ifndef BUTTON_TIMER_H
#define BUTTON_TIMER_H

#include "Arduino.h"
#include "driver/timer.h"

class ButtonTimer {
public:
    // 定义按键事件类型
    enum class Event {
        NONE,
        PRESS,
        RELEASE,
        LONG_PRESS_3S,
        LONG_PRESS_10S
    };

    // 构造函数：引脚号、检测间隔(微秒)、消抖时间(毫秒)
    ButtonTimer();
    
    // 初始化定时器和按键
    void begin(int pin);
    
    // 停止定时器
    void end();
    
    // 获取当前按键状态
    bool isPressed() const;

    void handleButtonEvent(ButtonTimer::Event event);

public:
    uint8_t _debounceMs;
    portMUX_TYPE _mux;
    int _pin;

    // 按键状态变量
    bool _currentState;
    bool _lastSteadyState;
    bool _lastFlickerableState;
    unsigned long _lastDebounceTime;
    unsigned long _pressStartTime;
    Event _lastEvent;

private:
    hw_timer_t* _timer;

    // 定时器中断处理函数
    static void IRAM_ATTR _onTimer(void *arg);
    
};

#endif // BUTTON_TIMER_H    