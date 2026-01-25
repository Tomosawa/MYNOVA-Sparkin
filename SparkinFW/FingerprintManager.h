/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
#ifndef FINGERPRINT_MANAGER_H
#define FINGERPRINT_MANAGER_H

#include <Arduino.h>
#include "Fingerprint.h"
#include "SleepManager.h"
#include "UnlockManager.h"
#include "BatteryManager.h"

class FingerprintManager {
public:
    FingerprintManager();
    void begin(SleepManager* sleep, UnlockManager* unlock);
    
private:
    static void taskFunction(void* param);
    
    TaskHandle_t _taskHandle;
    SleepManager* _sleepManager;
    UnlockManager* _unlockManager;
};

#endif
