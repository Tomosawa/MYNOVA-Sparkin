/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
#include "SleepManager.h"
#include "Sleep.h"
#include "Fingerprint.h"
#include "BluetoothManager.h"
#include "Common.h"
#include "IOPin.h"
#include "ButtonHandle.h"

extern Fingerprint fingerprint;
extern BluetoothManager bluetoothManager;
extern ConfigManager configManager;
extern ButtonHandler buttonHandler;
extern void handleTouchInterrupt();

SleepManager::SleepManager() 
    : _lastActivityTime(0), _bPreventSleep(false), _bSleepMode(false) {
}

void SleepManager::begin() {
    _lastActivityTime = millis();
    _bPreventSleep = false;
    _bSleepMode = false;
}

void SleepManager::resetActivity() {
    _lastActivityTime = millis();
    if (_bSleepMode) {
        // 如果处于休眠模式被调用（理论上不应该，除非中断唤醒后立即调用），标记退出
        _bSleepMode = false;
    }
}

void SleepManager::preventSleep(bool prevent) {
    _bPreventSleep = prevent;
    resetActivity();
}

void SleepManager::loop() {
    // 正在阻止休眠，或者已经在休眠模式，则不进行检查
    if (_bPreventSleep || _bSleepMode) {
        return;
    }
 
    // 检查是否在配对模式中（无绑定设备），防止配对时休眠
    if (bluetoothManager.isPairingMode()) {
        resetActivity();
        return;
    }

    uint32_t sleepTimeoutMs = configManager.getSleepTimeout() * 1000;
    
    // 检查超时
    if (sleepTimeoutMs > 0 && (millis() - _lastActivityTime) >= sleepTimeoutMs) {
        //进入休眠
        enterSleepMode();
    }

    // if (sleepTimeoutMs > 0 && (millis() - _lastActivityTime) >= sleepTimeoutMs) {
    //     // 首先看看蓝牙是否连接，如果连接发送MSG_CHECK_SLEEP消息确认是否可休眠
    //     if (bluetoothManager.isConnected()) {
    //         bluetoothManager.sendMessage(MSG_CHECK_SLEEP, &MSG_CMD_EXECUTE, 1);
    //         //等待蓝牙返回
    //         EventBits_t uxBits = xEventGroupWaitBits(
    //             event_group,
    //             EVENT_BIT_BLE_SLEEP_ENABLE,
    //             pdTRUE,
    //             pdTRUE,
    //             5000 / portTICK_PERIOD_MS
    //         );
    //         if (uxBits & EVENT_BIT_BLE_SLEEP_ENABLE) {
    //             // 检查是否是确认可休眠的消息
    //             if (bEnableSleep) {
    //                 // 确认可休眠，进入休眠模式
    //                 enterSleepMode();
    //             }
    //             else
    //             {
    //                 resetActivity();
    //             }
    //         }else{
    //             // 蓝牙通信超时，还是休眠
    //             enterSleepMode();
    //         }
    //     }
    //     else {
    //         // 如果蓝牙未连接，直接进入休眠模式
    //         enterSleepMode();
    //     }
    // }
}

void SleepManager::enterSleepMode() {
    Serial.println("[SLEEP]自动休眠时间已到，准备进入休眠模式...");
    _bSleepMode = true;
    
    // 禁用自动广播，防止断开连接后立即重连
    bluetoothManager.enableAutoAdvertising(false);

    // 停止按键任务和定时器，准备休眠
    buttonHandler.end();

    // 指纹模块休眠
    fingerprint.setPower(false);
    Serial.println("[SLEEP]指纹模块已进入休眠模式");

    // 取消中断
    detachInterrupt(digitalPinToInterrupt(PIN_FINGERPRINT_TOUCH));

    // 断开蓝牙
    if (bluetoothManager.isConnected()) {
        Serial.println("[SLEEP]蓝牙已经连接，尝试断开...");
        bluetoothManager.disconnectCurrentDevice();
        // 等待客户端断开连接
        EventBits_t uxBits = xEventGroupWaitBits(
            event_group,
            EVENT_BIT_BLE_DISCONNECTED,
            pdTRUE,
            pdTRUE,
            10000 / portTICK_PERIOD_MS
        );
        
        if (uxBits & EVENT_BIT_BLE_DISCONNECTED) {
            Serial.println("[SLEEP]蓝牙断开连接成功");
        } else {
            Serial.println("[SLEEP]蓝牙断开连接超时");
        }
    }

    // 进入轻度睡眠
    int wakeUpPin = enterLightSleep();
    
    // 唤醒后的处理
    wakeUp();
    
    // 如果是触摸唤醒，触发一次指纹识别逻辑（这里通过全局标志位传递给FingerprintManager）
    if(wakeUpPin == PIN_FINGERPRINT_TOUCH) {
        touchTriggered = true;
    }
}

void SleepManager::wakeUp() {
    Serial.println("[SLEEP]从休眠中唤醒");
    Serial.flush();
    
    Serial.println("[SLEEP]准备调用 buttonHandler.begin()");
    Serial.flush();
    
    // 1. 首先启动按键任务，确保能够检测按键状态
    //    即使按键在唤醒时已经按下，也能被检测到
    buttonHandler.begin();
    
    Serial.println("[SLEEP]buttonHandler.begin() 调用完成");
    Serial.flush();
    
    // 2. 恢复自动广播（只是设置标志位，不会阻塞）
    bluetoothManager.enableAutoAdvertising(true);

    // 3. 重新初始化指纹模块（阻塞操作）
    //    注意：这里会阻塞最多500ms，但按键任务已经在运行了
    fingerprint.setPower(true);
    fingerprint.waitStartSignal();
    
    // 4. 恢复中断
    attachInterrupt(digitalPinToInterrupt(PIN_FINGERPRINT_TOUCH), handleTouchInterrupt, RISING);
    
    resetActivity();
    _bSleepMode = false;
}
