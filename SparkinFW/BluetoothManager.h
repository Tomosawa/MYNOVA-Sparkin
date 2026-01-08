/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
#ifndef BLUETOOTH_MANAGER_H
#define BLUETOOTH_MANAGER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLESecurity.h>
#include <BLEHIDDevice.h>
#include <Preferences.h>
#include "BleKeyboard.h"
#include "Common.h"

// 定义服务和特征的UUID
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// 定义消息类型
static const uint8_t MSG_FINGERPRINT_SEARCH = 0x01;  // 指纹识别成功
static const uint8_t MSG_FINGERPRINT_REGISTER = 0x02;  // 指纹注册请求
static const uint8_t MSG_FINGERPRINT_DELETE = 0x03;    // 指纹清除请求
static const uint8_t MSG_DEVICE_NOTIFY = 0x04;  // 订阅成功的通知
static const uint8_t MSG_LOCKSCREEN_STATUS = 0x05;  // 锁屏状态
static const uint8_t MSG_PUT_FINGER = 0x06;  // 放入手指命令
static const uint8_t MSG_REMOVE_FINGER = 0x07;  // 移除手指命令
static const uint8_t MSG_GET_INFO = 0x08;  // 获取设备信息
static const uint8_t MSG_FINGERPRINT_REGISTER_CANCEL = 0x09;  // 取消指纹注册请求
static const uint8_t MSG_SET_SLEEPTIME = 0x10; //设置睡眠时间
static const uint8_t MSG_SET_FINGER_NAME = 0x20;   // 设置指纹名称
static const uint8_t MSG_GET_FINGER_NAMES = 0x21;  // 获取所有指纹名称
static const uint8_t MSG_RENAME_FINGER_NAME = 0x22; // 重命名指纹名称
static const uint8_t MSG_ENABLE_SLEEP = 0x23; // 是否启动自动休眠
static const uint8_t MSG_FIRMWARE_UPDATE_START = 0x24; //开始固件升级
static const uint8_t MSG_FIRMWARE_UPDATE_CHUNK = 0x25; //传输固件块
static const uint8_t MSG_FIRMWARE_UPDATE_END = 0x26; ///固件升级结束

static const uint8_t MSG_REST_ALL = 0x99; // 恢复出厂设置

// 定义消息值
static const uint8_t MSG_CMD_SUCCESS = 0xA1;  // 成功
static const uint8_t MSG_CMD_FAILURE = 0xA0; // 失败
static const uint8_t MSG_CMD_EXECUTE = 0xA2;  // 执行命令中
static const uint8_t MSG_CMD_CANCEL = 0xA3;  // 取消命令

// 定义数据结构体
#pragma pack(push)  // 保存当前对齐状态
#pragma pack(1)     // 设置为 1 字节对齐
typedef struct {
  uint8_t result; // 0：失败  1：成功
} MsgResult;
typedef struct {
  uint32_t sleepTime; // 睡眠时间设定
  char deviceId[20];
  char buildDate[10];
  char firmwareVer[10];
} MsgInfo;

typedef struct 
{
  uint8_t index;
  char fpName[MAX_FINGERNAME_LENGTH];
}FPData;

#pragma pack(pop)   // 恢复原有对齐状态

// 回调函数类型定义
typedef void (*MessageCallback)(uint8_t msgType, uint8_t* data, size_t length);

class BluetoothManager : public BLEServerCallbacks, public BLECharacteristicCallbacks {
private:
    BLEServer* pServer;
    BLEService* pService;
    BLECharacteristic* pCharacteristic;
    BLE2902* pBLE2902;
    BleKeyboard* pBleKeyboard;  // 添加BleKeyboard指针
    
    bool deviceConnected;
    MessageCallback messageCallback;
    Preferences preferences;  // 用于存储配对信息
    BLEAddress* pClientAddress; // 保存当前连接的客户端地址
    bool autoReconnect;       // 是否自动重连
    bool isAdvertising;       // 是否正在广播
    // 在BluetoothManager.h中修改onConnect方法声明
    SemaphoreHandle_t sendMutex;
    SemaphoreHandle_t stateMutex; // 添加状态互斥锁
    
private:
    // BLEServerCallbacks接口实现
    void onConnect(BLEServer *pServer, esp_ble_gatts_cb_param_t *param) override; // 添加这一行
    void onDisconnect(BLEServer* pServer) override;
    
    // BLECharacteristicCallbacks接口实现
    void onWrite(BLECharacteristic* pCharacteristic) override;
public:
    bool isConnectedNotify;   // 是否已连接通知

public:
    BluetoothManager();
    
    // 初始化蓝牙
    void begin(const char* deviceName, BleKeyboard* bleKeyboard);
    
    // 设置消息回调函数
    void setMessageCallback(MessageCallback callback);
    
    // 发送消息
    bool sendMessage(const uint8_t msgType, const uint8_t* data = nullptr, size_t length = 0);
    
    // 处理蓝牙事件，需要在loop中调用
    void update();
    
    // 检查是否已连接（更准确，直接查询BLE连接数）
    bool isConnected();
    bool isNotificationEnabled();
    // 检查是否在广播中
    bool checkAdvertising() { 
        bool advertising = false;
        if (stateMutex && xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
            advertising = isAdvertising;
            xSemaphoreGive(stateMutex);
        }
        return advertising;
    }

    // 启用/禁用自动重连
    void setAutoReconnect(bool enable) { 
        if (stateMutex && xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
            autoReconnect = enable;
            xSemaphoreGive(stateMutex);
        }
    }
    
    // 尝试连接到已保存的设备
    bool connectToPairedDevice();
    // 清除已保存的配对信息
    void clearPairedDevices();
    
    // 配对当前连接的设备
    bool pairDevice();
    
    // 开始广播
    void startAdvertising(bool bFastMode = true);
    
    // 停止广播
    void stopAdvertising();

    // 通知BleKeyboard连接状态变化
    void notifyKeyboardConnected();
    void notifyKeyboardDisconnected();

    // 检查并连接已配对设备
    bool checkAndConnect();

    void unpairDevice();
    void disconnectCurrentDevice();

    void setBatteryLevel(uint8_t level);
};

#endif // BLUETOOTH_MANAGER_H