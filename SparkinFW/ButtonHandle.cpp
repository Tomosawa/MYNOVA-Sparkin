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

extern BluetoothManager bluetoothManager;
extern ConfigManager configManager;
extern Fingerprint fingerprint;
ButtonTimer buttonTimer;

bool bRunTask = true;

TaskHandle_t g_buttonTaskHandle = nullptr;

ButtonHandler::ButtonHandler(uint8_t pin)
    : _pin(pin), _taskHandle(nullptr) {}

void ButtonHandler::begin()
{
    bRunTask = true;
    buttonTimer.begin(_pin);
    xTaskCreatePinnedToCore(buttonTask, "ButtonTask", 2048, this, 5, &g_buttonTaskHandle, 0);
    _taskHandle = g_buttonTaskHandle;
}

void ButtonHandler::end()
{
    bRunTask = false;
    buttonTimer.end();

    if (_taskHandle != nullptr)
    {
        vTaskDelete(_taskHandle);
        _taskHandle = nullptr;
    }
}

void ButtonHandler::buttonTask(void *pvParameters)
{
    ButtonHandler* handler = static_cast<ButtonHandler*>(pvParameters);
    while (bRunTask)
    {
        // 等待通知（阻塞，直到收到通知）
        uint32_t notifyValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (notifyValue == 0) continue; // 超时，实际不会发生

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
            Serial.println("[ButtonHandler] 3秒事件触发");
            bluetoothManager.unpairDevice();
            fingerprint.setLEDCmd(Fingerprint::LED_CODE_BLINK,0x07,(uint8_t)( (5 << 4) | 5 ), 0x00, 8);
            Serial.println("[ButtonHandler] 已清除配对信息，设备可被发现");
        }
        else if (notifyValue & BUTTON_RELEASE_3S)
        {
            Serial.println("[ButtonHandler] 3秒事件按键释放操作");
            bPairMode = true; // 设置为配对模式
            bluetoothManager.stopAdvertising();
            bluetoothManager.startAdvertising();
        }

    }
}