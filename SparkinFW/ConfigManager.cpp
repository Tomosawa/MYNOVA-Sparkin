/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
#include "ConfigManager.h"
#include "BluetoothManager.h"

const char* ConfigManager::NAMESPACE = "sparkin";
const char* ConfigManager::SLEEP_TIMEOUT_KEY = "sleep_time";
const char* ConfigManager::BLE_ADDRESS_KEY = "ble_addr";
const char* ConfigManager::FINGERPRINT_NAME_KEY_PREFIX = "fp_name_";

ConfigManager::ConfigManager() {}

bool ConfigManager::begin() {
    if (!prefs.begin(NAMESPACE, false)) {
        Serial.println("Failed to initialize Preferences");
        return false;
    }
    return true;
}

void ConfigManager::load()
{
    // 读取自动休眠时间
    sleepTimeout = prefs.getUInt(SLEEP_TIMEOUT_KEY, DEFAULT_SLEEP_TIMEOUT);
    Serial.printf("Loaded sleep timeout: %u seconds\n", sleepTimeout);

    // 读取BLE地址
    size_t len = prefs.getBytes(BLE_ADDRESS_KEY, bleAddress, 6);
    if (len == 6) {
        Serial.print("Loaded BLE address: ");
        for (int i = 0; i < 6; i++) {
            Serial.printf("%02X", bleAddress[i]);
            if (i < 5) Serial.print(":");
        }
        Serial.println();
    } else {
        Serial.println("No saved BLE address, will generate new one");
        memset(bleAddress, 0, 6);
    }

}

void ConfigManager::save() {
    // 保存自动休眠时间
    prefs.putUInt(SLEEP_TIMEOUT_KEY, sleepTimeout);
    Serial.printf("Saved sleep timeout: %u seconds\n", sleepTimeout);

    // 保存BLE地址（如果有）
    if (bleAddress[0] != 0 || bleAddress[1] != 0 || bleAddress[2] != 0 ||
        bleAddress[3] != 0 || bleAddress[4] != 0 || bleAddress[5] != 0) {
        prefs.putBytes(BLE_ADDRESS_KEY, bleAddress, 6);
        Serial.print("Saved BLE address: ");
        for (int i = 0; i < 6; i++) {
            Serial.printf("%02X", bleAddress[i]);
            if (i < 5) Serial.print(":");
        }
        Serial.println();
    }

}

void ConfigManager::setSleepTimeout(uint32_t seconds) {
    sleepTimeout = seconds;
    Serial.printf("Sleep timeout set to: %u seconds\n", sleepTimeout);
}

uint32_t ConfigManager::getSleepTimeout() {
    return sleepTimeout;
}

void ConfigManager::clearPairedDevices() {
    // 清除ESP32底层的所有绑定信息
    int dev_num = esp_ble_get_bond_device_num();
    if (dev_num > 0) {
        esp_ble_bond_dev_t *dev_list = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
        if (dev_list != nullptr) {
            esp_ble_get_bond_device_list(&dev_num, dev_list);
            for (int i = 0; i < dev_num; i++) {
                esp_ble_remove_bond_device(dev_list[i].bd_addr);
                Serial.printf("Removed bonded device %d\n", i);
            }
            free(dev_list);
        }
        Serial.println("Cleared all BLE bonded devices");
    } else {
        Serial.println("No bonded devices to clear");
    }
    clearBLEAddress();
}

bool ConfigManager::setFingerprintName(int id, const String& name) {
    char key[20];
    snprintf(key, sizeof(key), "%s%d", FINGERPRINT_NAME_KEY_PREFIX, id);
    prefs.putString(key, name);
    return true;
}

bool ConfigManager::getFingerprintName(int id, String& name) {
    char key[20];
    snprintf(key, sizeof(key), "%s%d", FINGERPRINT_NAME_KEY_PREFIX, id);
    key[sizeof(key) - 1] = '\0';
    name = prefs.getString(key, "");
    return name.length() > 0;
}

void ConfigManager::clearAllFingerprintNames() {
    // 全部清空
    for (int i = 0; i < MAX_FINGERPRINT_NUM; ++i) {
        char key[20];
        snprintf(key, sizeof(key), "%s%d", FINGERPRINT_NAME_KEY_PREFIX, i);
        prefs.remove(key);
    }
}

void ConfigManager::removeFingerprintName(int id) {
    char key[20];
    snprintf(key, sizeof(key), "%s%d", FINGERPRINT_NAME_KEY_PREFIX, id);
    key[sizeof(key) - 1] = '\0';
    prefs.remove(key);
}

bool ConfigManager::renameFingerprintName(int id, const String& newName) {
    return setFingerprintName(id, newName);
}

void ConfigManager::getAllFingerprintNames(std::vector<FPData>& names, uint8_t *indexTable) {
    names.clear();
    // 循环检查indexTable每一个字节有8bit，检查每一bit，如果是1则读取对应名称，否则跳过
    for(int i = 0; i < INDEX_TABLE_LENGTH; ++i) {
        uint8_t byte = indexTable[i];
        if (byte != 0) { // 只有当字节不为0时才检查每一位
            for(int j = 0; j < 8; ++j) {
                if (byte & (1 << j)) { // 检查第j位是否为1
                    int id = i * 8 + j;
                    if (id < MAX_FINGERPRINT_NUM) {
                        String name;
                        if (getFingerprintName(id, name)) {
                            FPData fpData = {0};
                            fpData.index = id;
                            strncpy(fpData.fpName, name.c_str(), MAX_FINGERNAME_LENGTH - 1);
                            fpData.fpName[MAX_FINGERNAME_LENGTH - 1] = '\0';
                            names.push_back(fpData);
                        } else {
                            Serial.printf("Fingerprint name not found for ID %d\n", id);
                        }
                    }
                }
            }
        }
    }
    Serial.printf("getAllFingerprintNames: Found %d names\n", names.size());
    // for (int i = 0; i < MAX_FINGERPRINT_NUM; ++i) {
    //     char key[20];
    //     snprintf(key, sizeof(key), "%s%d", FINGERPRINT_NAME_KEY_PREFIX, i);
    //     String name = prefs.getString(key, "");
    //     if (name.length() > 0) {
    //         names.push_back(name);
    //     } else {
    //         break;
    //     }
    // }
}

void ConfigManager::clear() {
    // 清除所有配置信息
    prefs.clear();
    sleepTimeout = DEFAULT_SLEEP_TIMEOUT;
    memset(bleAddress, 0, 6);

    // 清除底层BLE绑定
    clearPairedDevices();
    
    Serial.println("All configurations cleared.");
}


// BLE地址管理方法
bool ConfigManager::getBLEAddress(uint8_t addr[6]) {
    if (bleAddress[0] == 0 && bleAddress[1] == 0 && bleAddress[2] == 0 &&
        bleAddress[3] == 0 && bleAddress[4] == 0 && bleAddress[5] == 0) {
        return false;  // 没有保存的地址
    }
    memcpy(addr, bleAddress, 6);
    return true;
}

void ConfigManager::setBLEAddress(const uint8_t addr[6]) {
    memcpy(bleAddress, addr, 6);
    save();  // 立即保存
}

void ConfigManager::generateNewBLEAddress() {
    // 生成静态随机地址（Static Random Address）
    // 格式：最高两位必须是 11（0xC0 或 0xD0, 0xE0, 0xF0）
    
    // 使用ESP32的硬件随机数生成器
    esp_fill_random(bleAddress, 6);
    
    // 设置最高两位为 11（静态随机地址标识）
    bleAddress[0] |= 0xC0;
    
    Serial.print("Generated new BLE address: ");
    for (int i = 0; i < 6; i++) {
        Serial.printf("%02X", bleAddress[i]);
        if (i < 5) Serial.print(":");
    }
    Serial.println();
    
    save();  // 保存到NVS
}

bool ConfigManager::hasBLEAddress() {
    return !(bleAddress[0] == 0 && bleAddress[1] == 0 && bleAddress[2] == 0 &&
             bleAddress[3] == 0 && bleAddress[4] == 0 && bleAddress[5] == 0);
}

void ConfigManager::clearBLEAddress() {
    memset(bleAddress, 0, 6);
    prefs.remove(BLE_ADDRESS_KEY);
}