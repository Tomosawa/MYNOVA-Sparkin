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
#include "SleepManager.h"
#include "UnlockManager.h"
#include "FingerprintManager.h"

#define BLUETOOTH_NAME "Sparkin FP01"

// 定义类对象
Fingerprint fingerprint(PIN_FINGERPRINT_RX, PIN_FINGERPRINT_TX);  //指纹传感器对象
BluetoothManager bluetoothManager;                                //蓝牙管理器
BleKeyboard bleKeyboard;                                          //键盘对象
ConfigManager configManager;                                      //配置信息
VersionInfo versionInfo;                                          //版本信息
ButtonHandler buttonHandler(PIN_PAIR_BUTTON);                     //按键消息处理
BatteryManager batteryManager(PIN_BATTERY_ADC, PIN_BATTERY_TEST); //电池管理器
SleepManager sleepManager;                                        //休眠管理器
UnlockManager unlockManager;                                      //解锁管理器
FingerprintManager fingerprintManager;                            //指纹消息管理器

// 用于跟踪触摸引脚的上一个状态
int lastTouchState = LOW;

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
  pinMode(PIN_PAIR_BUTTON, INPUT_PULLUP);
  pinMode(PIN_FINGERPRINT_TOUCH, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(PIN_FINGERPRINT_TOUCH), handleTouchInterrupt, RISING);

  // 初始化休眠管理器
  sleepManager.begin();
  // 初始化解锁管理器
  unlockManager.begin(&sleepManager);
  // 初始化指纹消息管理器 
  fingerprintManager.begin(&sleepManager, &unlockManager);

  // 立即检查电池电量
  batteryManager.CheckBatteryLow();

  Serial.println("==========Init Completed!==========");
}

void loop() {
  // 更新蓝牙状态
  bluetoothManager.loop();
  
  // 休眠管理
  sleepManager.loop();

  delay(50);
}
