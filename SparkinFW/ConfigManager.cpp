/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
#include "ConfigManager.h"
#include "BluetoothManager.h"

const char* ConfigManager::NAMESPACE = "sparkin";
const char* ConfigManager::SLEEP_TIMEOUT_KEY = "sleep_time";
const char* ConfigManager::PAIRED_DEVICES_KEY = "paired_dev";
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

    // 读取配对设备信息
    pairedDevice = prefs.getString(PAIRED_DEVICES_KEY, "");
    if (pairedDevice.length() > 0) {
        Serial.println("Loaded paired device:");
        Serial.println(pairedDevice);
    } else {
        Serial.println("No paired devices found.");
    }
}

void ConfigManager::save() {
    // 保存自动休眠时间
    prefs.putUInt(SLEEP_TIMEOUT_KEY, sleepTimeout);
    Serial.printf("Saved sleep timeout: %u seconds\n", sleepTimeout);

    // 保存配对设备信息
    if (pairedDevice.length() > 0) {
        prefs.putString(PAIRED_DEVICES_KEY, pairedDevice);
        Serial.print("Saved paired device:");
        Serial.println(pairedDevice);
    } else {
        prefs.remove(PAIRED_DEVICES_KEY);
        Serial.println("Cleared paired devices.");
    }
}

void ConfigManager::setSleepTimeout(uint32_t seconds) {
    sleepTimeout = seconds;
    Serial.printf("Sleep timeout set to: %u seconds\n", sleepTimeout);
}

uint32_t ConfigManager::getSleepTimeout() {
    return sleepTimeout;
}

bool ConfigManager::setPairedDevice(String address) {
    pairedDevice = address;
    return true;
}

bool ConfigManager::getPairedDevice(String& address) {
    address = pairedDevice;
    if (address.length() > 0) {
        return true;
    } else {
        return false;
    }
}

void ConfigManager::clearPairedDevices() {
    pairedDevice = "";
    save();
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
    pairedDevice = "";
    sleepTimeout = DEFAULT_SLEEP_TIMEOUT;
    Serial.println("All configurations cleared.");
}