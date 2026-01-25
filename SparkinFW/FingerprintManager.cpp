/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
#include "FingerprintManager.h"
#include "Common.h"

extern Fingerprint fingerprint;
extern BatteryManager batteryManager;

FingerprintManager::FingerprintManager() 
    : _taskHandle(nullptr), _sleepManager(nullptr), _unlockManager(nullptr) {
}

void FingerprintManager::begin(SleepManager* sleep, UnlockManager* unlock) {
    _sleepManager = sleep;
    _unlockManager = unlock;
    
    xTaskCreate(
        taskFunction,
        "FingerprintTask",
        4096,
        this,
        1,
        &_taskHandle
    );
}

void FingerprintManager::taskFunction(void* param) {
    FingerprintManager* manager = static_cast<FingerprintManager*>(param);
    
    while (true) {
        if (touchTriggered) {
            // 重置睡眠时间
            if (manager->_sleepManager) {
                manager->_sleepManager->preventSleep(false); // 确保没有被意外阻止
                manager->_sleepManager->resetActivity();
            }
            
            Serial.println("[FP] IRQ detected! Auto searching fingerprint...");
            
            // 开始搜索验证指纹
            if (fingerprint.searchFingerprint()) {
                Serial.println("[FP] Match succeed!");
                
                // 请求解锁
                if (manager->_unlockManager) {
                    manager->_unlockManager->requestUnlock();
                }
            } else {
                Serial.println("[FP] Match fail!");
            }
            
            // 防止重复触发
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            
            // 检查电池电量
            batteryManager.CheckBatteryLow();

            // 重置中断标志
            touchTriggered = false;
        }
        
        // 这里的延时可以适当调整，用于轮询touchTriggered
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}
