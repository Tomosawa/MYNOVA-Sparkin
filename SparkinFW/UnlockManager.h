/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
#ifndef UNLOCK_MANAGER_H
#define UNLOCK_MANAGER_H

#include <Arduino.h>
#include "BluetoothManager.h"
#include "BleKeyboard.h"
#include "Common.h"
#include "SleepManager.h"

class UnlockManager {
public:
    UnlockManager();
    void begin(SleepManager* sleepManager);
    
    // 请求解锁，如果正在解锁中则返回false
    bool requestUnlock();
    
    // 是否正在忙于解锁
    bool isBusy();

private:
    static void taskFunction(void* param);
    void executeUnlockSequence();

    TaskHandle_t _taskHandle;
    QueueHandle_t _requestQueue;
    SleepManager* _sleepManager;
    bool _isBusy;
};

#endif
