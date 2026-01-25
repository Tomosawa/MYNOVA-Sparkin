/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
#include "ButtonHandle.h"
#include "ConfigManager.h"
#include "BluetoothManager.h"
#include "Fingerprint.h"
#include "ButtonTimer.h"
#include "IOPin.h"
#include "SleepManager.h"

extern BluetoothManager bluetoothManager;
extern ConfigManager configManager;
extern Fingerprint fingerprint;
extern SleepManager sleepManager;
ButtonTimer buttonTimer;

bool bRunTask = true;

TaskHandle_t g_buttonTaskHandle = nullptr;

// 按键中断服务函数
void IRAM_ATTR buttonISR() {
    if (g_buttonTaskHandle) {
        // 通知任务按键状态发生变化，立即唤醒
        // 使用 eNoAction 仅唤醒任务，不修改通知值，避免与位标志冲突
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xTaskNotifyFromISR(g_buttonTaskHandle, 0, eNoAction, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

ButtonHandler::ButtonHandler(uint8_t pin)
    : _pin(pin), _taskHandle(nullptr) {}

void ButtonHandler::begin()
{
    Serial.println("[ButtonHandler] begin() 开始执行");
    Serial.flush();
    
    bRunTask = true;
    
    Serial.println("[ButtonHandler] 准备初始化 ButtonTimer");
    Serial.flush();
    
    buttonTimer.begin(_pin);
    
    Serial.println("[ButtonHandler] ButtonTimer 初始化完成，准备附加中断");
    Serial.flush();
    
    // 附加中断，任何状态变化都唤醒任务
    attachInterrupt(digitalPinToInterrupt(_pin), buttonISR, CHANGE);
    
    Serial.println("[ButtonHandler] 中断已附加，准备创建任务");
    Serial.flush();

    // 创建按键处理任务
    BaseType_t ok = xTaskCreatePinnedToCore(buttonTask, "ButtonTask", 4096, this, 5, &g_buttonTaskHandle, 0);
    if (ok == pdPASS) {
        _taskHandle = g_buttonTaskHandle;
        Serial.println("[ButtonHandler] 任务创建成功");
    } else {
        g_buttonTaskHandle = nullptr;
        _taskHandle = nullptr;
        Serial.println("[ButtonHandler] 任务创建失败！");
    }
    Serial.flush();
}

void ButtonHandler::end()
{
    bRunTask = false;
    detachInterrupt(digitalPinToInterrupt(_pin)); // 取消中断
    buttonTimer.end();

    if (_taskHandle != nullptr)
    {
        vTaskDelete(_taskHandle);
        _taskHandle = nullptr;
    }
    g_buttonTaskHandle = nullptr;
}

void ButtonHandler::buttonTask(void *pvParameters)
{
    ButtonHandler* handler = static_cast<ButtonHandler*>(pvParameters);
    while (bRunTask)
    {
        // 计算等待时间：
        // 如果按键空闲（释放且稳定），则等待1秒（作为心跳/容错），而不是无限等待
        // 如果按键按下或正在消抖，则使用50ms轮询
        TickType_t xTicksToWait = buttonTimer.isIdle() ? (1000 / portTICK_PERIOD_MS) : (50 / portTICK_PERIOD_MS);

        // 等待通知
        uint32_t notifyValue = 0;
        BaseType_t notifyResult = xTaskNotifyWait(0, 0xFFFFFFFF, &notifyValue, xTicksToWait);

        // 如果超时且没有任何通知，说明是心跳唤醒
        if (notifyResult == pdFALSE && buttonTimer.isIdle()) {
            // 可选：打印心跳日志，用于调试任务是否存活
            // Serial.println("[ButtonTask] Heartbeat - Alive");
            continue; 
        }

        // 轮询按键状态
        buttonTimer.poll();

        if (notifyValue & BUTTON_NOTIFY_PRESS)
        {
            sleepManager.preventSleep(true);
        }
        if (notifyValue & BUTTON_NOTIFY_RELEASE)
        {
            sleepManager.preventSleep(false);
        }

        if (notifyValue & BUTTON_NOTIFY_10S)
        {
            Serial.println("[ButtonHandler] 10秒事件触发");
            configManager.clear();
            bluetoothManager.unpairDevice();
            fingerprint.clearAllLib();
            fingerprint.setLEDCmd(Fingerprint::LED_CODE_OFF,0,0,0x00);  // 关闭灯
            Serial.println("[ButtonHandler] 已恢复出厂设置，设备可被发现");
        }
        else if (notifyValue & BUTTON_RELEASE_10S)
        {
            Serial.println("[ButtonHandler] 10秒事件按键释放操作");
            bluetoothManager.stopAdvertising();
            bluetoothManager.startAdvertising();
        }
        else if (notifyValue & BUTTON_NOTIFY_3S)
        {
            Serial.println("[ButtonHandler] 3秒事件触发，请求执行取消配对...");
            
            // 发起异步请求，而不是直接调用 unpairDevice()
            bluetoothManager.requestUnpairDevice();
            
            Serial.println("[ButtonHandler] 已发送取消配对请求");
        }
        else if (notifyValue & BUTTON_RELEASE_3S)
        {
            Serial.println("[ButtonHandler] 3秒事件按键释放操作");
        }

    }
}