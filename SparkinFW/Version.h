/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
#ifndef H_VERSION_H
#define H_VERSION_H
#include <Arduino.h>

#define FIRMWARE_VERSION    "1.2"

struct VersionInfo {
    String deviceId;  // 设备ID
    String buildDate; // 构建日期
    String firmwareVersion; // 固件版本
};

#endif