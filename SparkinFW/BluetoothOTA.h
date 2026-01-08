/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
#ifndef BLUETOOTH_OTA_H
#define BLUETOOTH_OTA_H

#include <Arduino.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp32/rom/miniz.h> 

#define DECOMPRESS_EXTRA_SIZE TINFL_LZ_DICT_SIZE

class BluetoothOTA {
private:
    const esp_partition_t* update_partition;
    esp_ota_handle_t update_handle;
    uint32_t calculated_crc32;
    uint32_t bytes_received;// 已接收数据大小
    uint32_t bytes_total; // 总的接收数据大小
    uint32_t bytes_decompressed; // 已解压数据大小
    
    tinfl_decompressor inflator;    // miniz解压缩器
    uint8_t* decompress_buffer;
    uint8_t* decompress_buffer_pos;
    const size_t DECOMPRESS_BUFF_SIZE = TINFL_LZ_DICT_SIZE + DECOMPRESS_EXTRA_SIZE; // 32K+2K的解压缓冲区
    size_t in_pos = 0;
    size_t out_size = 0;
    
public:
    BluetoothOTA();
    ~BluetoothOTA();
    
    // 开始OTA
    bool begin(uint32_t total_size);
    
    // 接收数据
    bool receiveData(const uint8_t* data, size_t length);
    
    // 完成并验证CRC32
    bool finish(String targetCRC32);
    
    // 获取接收的字节数
    uint32_t getBytesReceived() const { return bytes_received; }
};

#endif