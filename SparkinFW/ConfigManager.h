/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Preferences.h>
#include <Arduino.h>
#include "BluetoothManager.h"

class ConfigManager {
public:
    ConfigManager();
    
    // 初始化配置管理器
    bool begin();

    // 读取所有配置信息
    void load();
    // 保存所有配置信息
    void save();
    // 清除所有信息
    void clear();
    
    // 自动休眠时间相关方法
    void setSleepTimeout(uint32_t seconds);
    uint32_t getSleepTimeout();
    
    // 配对设备相关方法
    void clearPairedDevices();
    
    // BLE地址管理
    bool getBLEAddress(uint8_t addr[6]);  // 获取保存的BLE地址
    void setBLEAddress(const uint8_t addr[6]);  // 保存BLE地址
    void generateNewBLEAddress();  // 生成并保存新的随机地址
    bool hasBLEAddress();  // 检查是否有保存的地址
    void clearBLEAddress();  // 清除BLE地址

    // 指纹名称相关方法
    bool setFingerprintName(int id, const String& name); // 设置指定ID的指纹名称
    bool getFingerprintName(int id, String& name);       // 获取指定ID的指纹名称
    void clearAllFingerprintNames();                     // 清空所有指纹名称
    void removeFingerprintName(int id);                  // 删除指定ID的指纹名称（后面前移）
    bool renameFingerprintName(int id, const String& newName); // 重命名指定ID的指纹名称
    void getAllFingerprintNames(std::vector<FPData>& names,uint8_t* indexTable);   // 获取所有指纹名称

private:
    int sleepTimeout; // 自动休眠时间（秒）
    uint8_t bleAddress[6]; // BLE地址缓存

private:
    Preferences prefs;
    static const char* NAMESPACE;
    static const char* SLEEP_TIMEOUT_KEY;
    static const char* BLE_ADDRESS_KEY;
    static const char* FINGERPRINT_NAME_KEY_PREFIX; // 指纹名称key前缀
    static const int MAX_FINGERPRINT_NAME_LEN = 32; // UTF-8定长存储
    static const uint32_t DEFAULT_SLEEP_TIMEOUT = 10; // 默认10s休眠
};

#endif // CONFIG_MANAGER_H
