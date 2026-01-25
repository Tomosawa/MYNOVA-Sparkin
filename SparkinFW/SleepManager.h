/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
#ifndef SLEEP_MANAGER_H
#define SLEEP_MANAGER_H

#include <Arduino.h>
#include "ConfigManager.h"

class SleepManager {
public:
    SleepManager();
    void begin();
    void loop();
    
    // 重置最后活动时间
    void resetActivity();
    
    // 阻止或允许休眠 (例如按键按下时阻止, 临时)
    void preventSleep(bool prevent);
    
    // 检查是否应该休眠
    bool shouldSleep();
    
    // 进入休眠
    void enterSleepMode();
    
    // 从休眠中唤醒的处理
    void wakeUp();

    bool isSleepMode() const { return _bSleepMode; }

private:
    uint32_t _lastActivityTime;
    bool _bPreventSleep;
    bool _bSleepMode;
};

#endif
