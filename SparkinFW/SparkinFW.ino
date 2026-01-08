/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
#include <Arduino.h>
#include "esp_sleep.h"
#include "driver/rtc_io.h"

#include "IOPin.h"
#include "Fingerprint.h"
#include "BluetoothManager.h"
#include "BleKeyboard.h"
#include "BluetoothHandle.h"
#include "Common.h"
#include "ConfigManager.h"
#include "Sleep.h"
#include "Version.h"
#include "ButtonHandle.h"
#include "BatteryManager.h"

#define BLUETOOTH_NAME "Sparkin FP01"
// 定义类对象
Fingerprint fingerprint(PIN_FINGERPRINT_RX, PIN_FINGERPRINT_TX);  //指纹传感器
BluetoothManager bluetoothManager;                                //蓝牙管理器
BleKeyboard bleKeyboard;                                          //键盘对象
ConfigManager configManager;                                      // 配置信息
VersionInfo versionInfo;                                          // 版本信息
ButtonHandler buttonHandler(PIN_PAIR_BUTTON);
BatteryManager batteryManager(PIN_BATTERY_ADC, PIN_BATTERY_TEST); // 电池管理器
// 记录最后活动时间
uint32_t lastActivityTime = 0;

// 用于跟踪触摸引脚的上一个状态
int lastTouchState = LOW;
// 用于防止重复触发
bool isProcessingFingerprint = false;

// 配对按钮状态
uint32_t pairButtonPressStartTime = 0;
bool pairButton3sTriggered = false;
bool pairButton10sTriggered = false;

void CheckBatteryLow();

void setup() {
  // 初始化串口
  Serial.begin(115200);
  Serial.println(">>>>>>>>>>Sparkin Fingerprint Started!<<<<<<<<<<");

  // 版本信息
  versionInfo.deviceId = String(ESP.getEfuseMac(), HEX);
  versionInfo.buildDate = String(__DATE__);
  versionInfo.firmwareVersion = String(FIRMWARE_VERSION);
  String device_info = "FIRMWARE:\tV" + versionInfo.firmwareVersion + " \r\nBUILD DATE:\t" + versionInfo.buildDate + " \r\nDEVICE ID:\t" + versionInfo.deviceId;
  Serial.println(device_info);

  // 初始化配置管理器
  configManager.begin();
  configManager.load();
  // 初始化指纹模组
  fingerprint.begin(57600);
  fingerprint.setPower(true);  // 开启指纹模组电源
  fingerprint.waitStartSignal();  // 等待指纹模组启动信号
  // 上电打印模组基本参数
  fingerprint.readInfo();
  // 设置初始等待颜色
  fingerprint.setLEDCmd(Fingerprint::LED_CODE_BLINK,0x06,0x11,0x00,4);  // 黄色闪烁灯

  // 初始化最后活动时间
  lastActivityTime = millis();

  // 检查电池电量初始化
  batteryManager.init();

  // 初始化蓝牙
  bluetoothManager.begin(BLUETOOTH_NAME, &bleKeyboard);
  bluetoothManager.setMessageCallback(handleBluetoothMessage);
  bluetoothManager.setAutoReconnect(true);  // 启用自动重连

  // 按键事件
  buttonHandler.begin();

  // 初始化事件组
  init_event_group();

  // 引脚初始化
  pinMode(PIN_FINGERPRINT_TOUCH, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(PIN_FINGERPRINT_TOUCH), handleTouchInterrupt, RISING);

  // 检查电池电量
  CheckBatteryLow();

  Serial.println("==========Init Completed!==========");
}

void loop() {
  // 更新蓝牙状态
  bluetoothManager.update();

  // 检查是否在配对中，否则重置时间不休眠
  if (bPairMode) {
    lastActivityTime = millis();  // 重置最后活动时间
  }

  // 检查是否需要进入休眠模式
  uint32_t sleepTimeoutMs = configManager.getSleepTimeout() * 1000;  // 转换为毫秒
  if (bEnableSleep && sleepTimeoutMs > 0 && (millis() - lastActivityTime) >= sleepTimeoutMs) {
    Serial.println("[SLEEP]自动休眠时间已到，准备进入休眠模式...");
    bSleepMode = true;  // 设置进入睡眠模式标志

    // 首先让指纹模块进入休眠
    fingerprint.setPower(false);
    Serial.println("[SLEEP]指纹模块已进入休眠模式");

    // 睡眠之前要取消中断，避免冲突
    detachInterrupt(digitalPinToInterrupt(PIN_FINGERPRINT_TOUCH));
    // 睡眠前主动断开BLE连接，加快主机端感知
    if (bluetoothManager.isConnected()) {
      Serial.println("[SLEEP]蓝牙已经连接，尝试断开...");

      bluetoothManager.disconnectCurrentDevice();
      EventBits_t uxBits = xEventGroupWaitBits(
        event_group,                 // 事件组句柄
        EVENT_BIT_BLE_DISCONNECTED,  // 等待的事件位
        pdTRUE,                      // 等待成功后清除事件位
        pdTRUE,                      // 使用逻辑与（所有位都置位才返回）
        10000 / portTICK_PERIOD_MS   // 等待10秒
      );
      if (uxBits & EVENT_BIT_BLE_DISCONNECTED) {
        Serial.println("[SLEEP]蓝牙断开连接成功");
      } else {
        Serial.println("[SLEEP]蓝牙断开连接超时");
      }
    }

    // 关闭指纹模块电源 =》 断电了已经，不需要单独关闭
    //fingerprint.setLEDCmd(Fingerprint::LED_CODE_OFF,0,0,0x00);  // 关闭呼吸灯

    // ESP32进入轻度睡眠
    int wakeUpPin = enterLightSleep(); 

    // 重新初始化指纹模块
    fingerprint.setPower(true);  // 开启指纹模组电源
    fingerprint.waitStartSignal();  // 等待指纹模组启动信号
    // 唤醒后立即广播
    bluetoothManager.startAdvertising(true);
    // 被唤醒后重新启用中断
    attachInterrupt(digitalPinToInterrupt(PIN_FINGERPRINT_TOUCH), handleTouchInterrupt, RISING);
    
    // 触发一次指纹识别响应，如果是触摸唤醒
    if(wakeUpPin == PIN_FINGERPRINT_TOUCH)
      touchTriggered = true;

    // 被唤醒后重置活动时间
    lastActivityTime = millis();
    bSleepMode = false;  // 重置睡眠模式标志
  }

  // 检测触摸中断信号
  if (touchTriggered) {
    bEnableSleep = true; // 重置 - 允许休眠
    lastActivityTime = millis();  // 更新最后活动时间
    Serial.println("[FP] IRQ detected! Auto searching fingerprint...");

    // 开始搜索验证指纹
    if (fingerprint.searchFingerprint()) {
      Serial.println("[FP] Match succeed!");
      
      // 蓝牙如果没有连接，则需要等待连接成功
      if (!bluetoothManager.isConnected()) {
        //蓝牙没有连接上，需要最多等待10秒蓝牙连接上
        Serial.println("[FP] 蓝牙未连接，等待连接...");
        // 等待蓝牙连接，最多等待10秒
        EventBits_t uxBits = xEventGroupWaitBits(
          event_group,                // 事件组句柄
          EVENT_BIT_BLE_CONNECTED,    // 等待的事件位
          pdTRUE,                     // 等待成功后清除事件位
          pdTRUE,                     // 使用逻辑与（所有位都置位才返回）
          10000 / portTICK_PERIOD_MS  // 等待10秒
        );
        if (uxBits & EVENT_BIT_BLE_CONNECTED) {
          Serial.println("[FP] 蓝牙连接成功");
        } else {
          Serial.println("[FP] 蓝牙连接超时");
          touchTriggered = false;  // 重置中断标志
          return;
        }
      }

      // 蓝牙连接成功后，无论何时，都需要发送按钮激活电脑
      bleKeyboard.write(KEY_LEFT_CTRL);

      if(!bluetoothManager.isNotificationEnabled()) {
        Serial.println("[FP] 蓝牙未订阅，等待订阅");
        // 等待订阅成功的消息
        EventBits_t notifyBits = xEventGroupWaitBits(
          event_group,                // 事件组句柄
          EVENT_BIT_BLE_NOTIFY,       // 等待的事件位
          pdTRUE,                     // 等待成功后清除事件位
          pdTRUE,                     // 使用逻辑与（所有位都置位才返回）
          10000 / portTICK_PERIOD_MS  // 等待10秒
        );
        if (!(notifyBits & EVENT_BIT_BLE_NOTIFY)) {
          Serial.println("[FP] 订阅消息通道未返回，无法发送指纹识别消息");
          delay(1000);
          touchTriggered = false;  // 重置中断标志
          return;
        }
        Serial.println("[FP] 收到了订阅成功消息，继续发送指纹解锁消息");
      } else {
        //先发送一个消息，看电脑是否有返回
        Serial.println("[FP] 蓝牙已连接，发送消息查询电脑是否在锁屏状态");
        uint8_t result = 1;
        bluetoothManager.sendMessage(MSG_LOCKSCREEN_STATUS, &result, 1);

        // 等待电脑返回
        EventBits_t uxBits = xEventGroupWaitBits(
          event_group,              // 事件组句柄
          EVENT_BIT_SCREENLOCK,     // 等待的事件位
          pdTRUE,                   // 等待成功后清除事件位
          pdTRUE,                   // 使用逻辑与（所有位都置位才返回）
          500 / portTICK_PERIOD_MS  // 等待500ms
        );
        // 发现没有返回，则发送按钮尝试激活电脑。
        if (!(uxBits & EVENT_BIT_SCREENLOCK)) {
          Serial.println("[FP] 等待电脑响应超时，可能休眠了，尝试发送按键唤醒电脑");
          bleKeyboard.write(KEY_LEFT_CTRL);
          delay(1000);
        }
      }

      //如果在锁屏状态，则发送解锁消息
      Serial.println("[FP] 发送指纹解锁屏幕命令");
      uint8_t result = 1;
      bluetoothManager.sendMessage(MSG_FINGERPRINT_SEARCH, &result, 1);

    } else {
      Serial.println("[FP] Match fail!");
    }
    // 防止重复触发
    delay(1000);
    touchTriggered = false;  // 重置中断标志
    // 检查电池电量
    CheckBatteryLow();
  }

  delay(50);  // 减小延迟以提高对IRQ信号的响应速度
}

void CheckBatteryLow(){
  float batteryVoltage = batteryManager.readVoltage();
  batteryPercentage = batteryManager.calculateBatteryPercent(batteryVoltage);
  bluetoothManager.setBatteryLevel((uint8_t)batteryPercentage);
  Serial.printf("Battery Voltage: %.2f V, Percentage: %.2f%%\n", batteryVoltage, batteryPercentage);
  // 如果电池电量低于阈值，发出警告
  if (batteryPercentage < 20.0) {
    fingerprint.setLEDCmd(Fingerprint::LED_CODE_BLINK,0x04,0x11,0x00,12);  // 红色闪烁灯
    Serial.println(">>>Warning<<<: Battery level is low! Please charge the device.");
  }
}