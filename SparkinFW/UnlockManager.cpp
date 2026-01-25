/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
#include "UnlockManager.h"

extern BluetoothManager bluetoothManager;
extern BleKeyboard bleKeyboard;
extern EventGroupHandle_t event_group;

UnlockManager::UnlockManager() 
    : _taskHandle(nullptr), _requestQueue(nullptr), _sleepManager(nullptr), _isBusy(false) {
}

void UnlockManager::begin(SleepManager* sleepManager) {
    _sleepManager = sleepManager;
    _requestQueue = xQueueCreate(1, sizeof(uint8_t)); // 队列长度1，只需要一个触发信号
    
    // 创建任务
    xTaskCreate(
        taskFunction,
        "UnlockTask",
        4096,
        this,
        1, // 优先级适中
        &_taskHandle
    );
}

bool UnlockManager::requestUnlock() {
    if (_isBusy) {
        Serial.println("[Unlock] 正在执行解锁任务，忽略新的请求");
        return false;
    }
    
    uint8_t dummy = 1;
    // 发送请求到队列，非阻塞
    if (xQueueSend(_requestQueue, &dummy, 0) == pdTRUE) {
        return true;
    }
    return false;
}

bool UnlockManager::isBusy() {
    return _isBusy;
}

void UnlockManager::taskFunction(void* param) {
    UnlockManager* manager = static_cast<UnlockManager*>(param);
    uint8_t dummy;
    
    while (true) {
        // 等待请求
        if (xQueueReceive(manager->_requestQueue, &dummy, portMAX_DELAY) == pdTRUE) {
            manager->_isBusy = true;
            manager->executeUnlockSequence();
            manager->_isBusy = false;
        }
    }
}

void UnlockManager::executeUnlockSequence() {
    Serial.println("[Unlock] 开始执行解锁序列");
    
    // 防止过程中休眠
    if (_sleepManager) _sleepManager->resetActivity();

    // 1. 检查蓝牙连接
    if (!bluetoothManager.isConnected()) {
        Serial.println("[Unlock] 蓝牙未连接，等待连接...");
        // 等待蓝牙连接，最多等待10秒
        EventBits_t uxBits = xEventGroupWaitBits(
            event_group,
            EVENT_BIT_BLE_CONNECTED,
            pdTRUE,
            pdTRUE,
            10000 / portTICK_PERIOD_MS
        );
        
        if (uxBits & EVENT_BIT_BLE_CONNECTED) {
            Serial.println("[Unlock] 蓝牙连接成功");
        } else {
            Serial.println("[Unlock] 蓝牙连接超时");
            return; // 退出序列
        }
    }

    // 2. 发送唤醒按键 (Left Ctrl)
    Serial.println("[Unlock] 发送唤醒按键");
    bleKeyboard.write(KEY_LEFT_CTRL);

    // 3. 检查通知订阅状态
    if (!bluetoothManager.isNotificationEnabled()) {
        Serial.println("[Unlock] 蓝牙未订阅，等待订阅");
        EventBits_t notifyBits = xEventGroupWaitBits(
            event_group,
            EVENT_BIT_BLE_NOTIFY,
            pdTRUE,
            pdTRUE,
            10000 / portTICK_PERIOD_MS
        );
        
        if (!(notifyBits & EVENT_BIT_BLE_NOTIFY)) {
            Serial.println("[Unlock] 订阅消息通道未返回，无法发送指纹识别消息");
            return;
        }
        Serial.println("[Unlock] 收到了订阅成功消息");
    } else {
        // 尝试查询锁屏状态
        Serial.println("[Unlock] 蓝牙已连接，发送消息查询电脑是否在锁屏状态");
        uint8_t result = 1;
        bluetoothManager.sendMessage(MSG_LOCKSCREEN_STATUS, &result, 1);

        EventBits_t uxBits = xEventGroupWaitBits(
            event_group,
            EVENT_BIT_SCREENLOCK,
            pdTRUE,
            pdTRUE,
            500 / portTICK_PERIOD_MS
        );
        
        if (!(uxBits & EVENT_BIT_SCREENLOCK)) {
            Serial.println("[Unlock] 等待电脑响应超时，可能休眠了，尝试再次发送按键唤醒电脑");
            bleKeyboard.write(KEY_LEFT_CTRL);
            delay(1000);
        }
    }

    // 4. 发送解锁命令
    Serial.println("[Unlock] 发送指纹解锁屏幕命令");
    uint8_t result = 1;
    bluetoothManager.sendMessage(MSG_FINGERPRINT_SEARCH, &result, 1);

    // 5. 解锁完成后，允许休眠
    if (_sleepManager) _sleepManager->preventSleep(false);

    Serial.println("[Unlock] 解锁序列完成");
}
