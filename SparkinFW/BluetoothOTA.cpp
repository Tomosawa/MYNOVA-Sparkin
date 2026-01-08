/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
#include "BluetoothOTA.h"
#include <rom/crc.h>

BluetoothOTA::BluetoothOTA() {
    update_partition = nullptr;
    update_handle = 0;
    bytes_received = 0;
    bytes_total = 0;
    bytes_decompressed = 0;
    calculated_crc32 = 0;
    decompress_buffer = nullptr;
    decompress_buffer_pos = nullptr;
}

BluetoothOTA::~BluetoothOTA() {
    if (update_handle) {
        esp_ota_abort(update_handle);
    }
    if (decompress_buffer != nullptr) {
        free(decompress_buffer);
        decompress_buffer = nullptr;  // 避免野指针
    }
}
bool BluetoothOTA::begin(uint32_t total_size) {
    Serial.println("Starting Bluetooth OTA...");
    // 获取OTA更新分区
    update_partition = esp_ota_get_next_update_partition(nullptr);
    if (!update_partition) {
        Serial.println("ERROR: No OTA partition found");
        return false;
    }
    
    Serial.print("OTA partition found: ");
    Serial.print(update_partition->label);
    Serial.print(", size: ");
    Serial.println(update_partition->size);
    Serial.print("Firmware size: ");
    Serial.println(total_size);
    
    // 开始OTA操作
    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (err != ESP_OK) {
        Serial.print("ERROR: esp_ota_begin failed: ");
        Serial.println(esp_err_to_name(err));
        return false;
    }
    
    // 重置状态
    bytes_received = 0;
    bytes_total = total_size;
    bytes_decompressed = 0;
    calculated_crc32 = 0;

    Serial.print("Free heap: ");
    Serial.println(ESP.getFreeHeap());
    // 初始化解压缩相关
    if(decompress_buffer == nullptr){
        Serial.print("Allocating decompress buffer...");
        decompress_buffer = (uint8_t*)malloc(DECOMPRESS_BUFF_SIZE);
        if (decompress_buffer == nullptr) {
            Serial.println("ERROR: Failed to allocate decompress buffer");
            return false;  // 分配失败直接返回，不标记为初始化完成
        }
    }
    Serial.print("Free heap: ");
    Serial.println(ESP.getFreeHeap());
    decompress_buffer_pos = decompress_buffer;
    in_pos = 0;
    out_size = DECOMPRESS_BUFF_SIZE;

    tinfl_init(&inflator);
    Serial.println("OTA begin successful");
    return true;
}

bool BluetoothOTA::receiveData(const uint8_t *data, size_t length)
{
    if (!update_handle)
    {
        Serial.println("ERROR: OTA not started");
        return false;
    }
    if (!decompress_buffer)
    {
        Serial.println("ERROR: Decompress buffer invalid");
        return false;
    }
    if (!data || length == 0)
    {
        Serial.println("ERROR: Invalid data (null or 0 length)");
        return false;
    }

    /* 1. 统计进度 */
    bytes_received += length;
    Serial.println("Received data chunk, size: " + String(length) + ", total received: " + String(bytes_received) + "/" + String(bytes_total));

    // 判断是否最后一块，设置flag
    uint32_t flags = 0;
    if (bytes_received < bytes_total)
    {
        flags |= TINFL_FLAG_HAS_MORE_INPUT;
    }

    /* 2. 定义输入流的指针和剩余长度 */
    in_pos = 0;
    while (in_pos < length)
    {
        // 剩余缓存可用大小
        size_t decompress_free_space = DECOMPRESS_BUFF_SIZE - (decompress_buffer_pos - decompress_buffer);
        out_size = decompress_free_space; // 可用空间
        //Serial.printf("Decompressing chunk, in_pos: %u, length: %u, decompress_free_space: %u\n",
        //             in_pos, length, decompress_free_space);
        // 如果剩余空间不足4K, 则开始内存移动
        if (decompress_free_space < 4096)
        {
            // 从当前位置的前面32K数据移动到开始位置
            memmove(decompress_buffer, decompress_buffer_pos - TINFL_LZ_DICT_SIZE, TINFL_LZ_DICT_SIZE);
            decompress_buffer_pos = decompress_buffer + TINFL_LZ_DICT_SIZE; // 更新写指针位置
            out_size = DECOMPRESS_EXTRA_SIZE;
            Serial.printf("Free space insufficient %d. Moved 32K bytes\n", decompress_free_space);
        }

        size_t in_consumed = length - in_pos;
        //Serial.printf("Before tinfl_decompress in_consumed: %u, out_size: %u, decompress_buff: %u, decompress_buffer_pos: %u\n",
        //             in_consumed, out_size, decompress_buffer, decompress_buffer_pos);
        tinfl_status status = tinfl_decompress(&inflator,
                                               data + in_pos, &in_consumed,
                                               decompress_buffer, decompress_buffer_pos, &out_size,
                                               flags);
        //Serial.printf("After tinfl_decompress in_consumed: %u, out_size: %u, decompress_buff: %u, decompress_buffer_pos: %u\n",
        //     in_consumed, out_size, decompress_buffer, decompress_buffer_pos);
        in_pos += in_consumed;

        //Serial.printf("tinfl_decompress returned status: %d, in_consumed: %u, out_produced: %u\n",
        //              status, in_consumed, out_size);
        if (status < TINFL_STATUS_DONE)
        {
            Serial.printf("Decompress failed with status: %d\n", status);
            return false;
        }

        /* 4. 处理解压后的数据 */
        if (out_size > 0)
        {
            // >>>>>>>>>>>>>打印解压后的数据<<<<<<<<<<<<<<<<
            // for (int i = 0; i < out_size; i++)
            // {
            //     // 确保单个字节显示为两位十六进制数
            //     if (decompress_buffer_pos[i] < 0x10)
            //     {
            //         Serial.print("0");
            //     }
            //     Serial.print(decompress_buffer_pos[i], HEX);
            //     Serial.print(" ");

            //     // 每16个字节换行，方便查看
            //     if ((i + 1) % 16 == 0)
            //     {
            //         Serial.println();
            //     }
            // }
            // Serial.println();
            // >>>>>>>>>>>>>调试信息<<<<<<<<<<<<<<<<

            // 统计解压后数据大小
            bytes_decompressed += out_size;
            // 计算CRC32校验和
            calculated_crc32 = crc32_le(calculated_crc32, decompress_buffer_pos, out_size);
            // 写入flash
            esp_err_t err = esp_ota_write(update_handle, decompress_buffer_pos, out_size);
            if (err != ESP_OK)
            {
                Serial.printf("OTA write failed: %s\n", esp_err_to_name(err));
                return false;
            }
        }

        decompress_buffer_pos += out_size; // 更新写指针位置

        // 如果解压完成，可以提前退出
        if (status == TINFL_STATUS_DONE)
        {
            Serial.println("Decompression finished.");
            break;
        }
    }

    return true;
}

bool BluetoothOTA::finish(String targetCRC32) {
    if (!update_handle) {
        Serial.println("ERROR: OTA not started");
        return false;
    }

    if (decompress_buffer != nullptr) {
        free(decompress_buffer);
        decompress_buffer = nullptr;  // 避免野指针
    }
    // 完成CRC32计算
    uint32_t final_crc = calculated_crc32;// ^ 0xFFFFFFFF;
    
    // 将目标CRC32字符串转换为数值
    uint32_t target_crc = strtoul(targetCRC32.c_str(), nullptr, 16);
    
    Serial.print("Calculated CRC32: 0x");
    Serial.println(final_crc, HEX);
    Serial.print("Target CRC32: 0x");
    Serial.println(target_crc, HEX);
    Serial.print("Total bytes received: ");
    Serial.println(bytes_received);
    Serial.print("Decompressed size: ");
    Serial.println(bytes_decompressed);
    
    // 验证CRC32
    if (final_crc != target_crc) {
        Serial.println("ERROR: CRC32 mismatch! OTA failed.");
        esp_ota_abort(update_handle);
        update_handle = 0;
        return false;
    }
    
    Serial.println("CRC32 verification passed!");
    
    // 结束OTA写入
    esp_err_t err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        Serial.print("ERROR: esp_ota_end failed: ");
        Serial.println(esp_err_to_name(err));
        update_handle = 0;
        return false;
    }
    
    update_handle = 0;
    
    // 设置新的启动分区
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        Serial.print("ERROR: esp_ota_set_boot_partition failed: ");
        Serial.println(esp_err_to_name(err));
        return false;
    }
    
    Serial.println("OTA completed successfully!");
    Serial.println("New firmware will be loaded after restart");
    
    return true;
}