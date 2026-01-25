/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
#include "BluetoothHandle.h"
#include "Fingerprint.h"
#include "BluetoothManager.h"
#include "ConfigManager.h"
#include "Version.h"
#include "IOPin.h"
#include "Common.h"
#include "BluetoothOTA.h"
#include "SleepManager.h"

extern Fingerprint fingerprint;
extern BluetoothManager bluetoothManager;
extern ConfigManager configManager;
extern VersionInfo versionInfo;
extern bool bCancelRegister;
extern SleepManager sleepManager;
BluetoothOTA bluetoothOTA;

#define BLUETOOTH_TASK_STACK_SIZE 4096
#define BLUETOOTH_TASK_PRIORITY 2
#define BLUETOOTH_QUEUE_LENGTH 20

// 全局队列句柄
static QueueHandle_t bluetoothMsgQueue = NULL;

// 蓝牙消息处理任务（只启动一次）
void bluetoothMessageQueueTask(void* pvParameters) {
    while (true) {
        TaskParameters* params = nullptr;
        if (xQueueReceive(bluetoothMsgQueue, &params, portMAX_DELAY) == pdPASS && params != nullptr) {
            bluetoothMessageTask(params); // 复用原有处理逻辑
        }
    }
}

// 初始化队列和处理任务（需在setup或初始化流程中调用一次）
void initBluetoothMessageQueue() {
    if (!bluetoothMsgQueue) {
        bluetoothMsgQueue = xQueueCreate(BLUETOOTH_QUEUE_LENGTH, sizeof(TaskParameters*));
        if (bluetoothMsgQueue) {
            xTaskCreate(
                bluetoothMessageQueueTask,
                "BLEQueueTask",
                BLUETOOTH_TASK_STACK_SIZE,
                NULL,
                BLUETOOTH_TASK_PRIORITY,
                NULL
            );
        }
    }
}
extern long recordTime;
// 任务处理函数
void bluetoothMessageTask(TaskParameters* params) {
    // 使用try-catch确保params始终被释放
    try {
        switch (params->msgType) {
            case MSG_FINGERPRINT_SEARCH:{
                Serial.println("[Task] Processing fingerprint search");
                break;
            }
            case MSG_FINGERPRINT_REGISTER:{
                Serial.println("[Task] Processing fingerprint registration");
                if (params->length < 1) {
                    Serial.println("[Task] Invalid fingerprint registration data");
                    bluetoothManager.sendMessage(MSG_FINGERPRINT_REGISTER, &MSG_CMD_FAILURE, 1);
                    break;
                }
                // 开始注册指纹之前需要取消中断
                detachInterrupt(digitalPinToInterrupt(PIN_FINGERPRINT_TOUCH));
                // 重置取消标志
                bCancelRegister = false;
                int fingerprintId = params->data[0];
                bool success = fingerprint.registerFingerprint(fingerprintId);
                // 注册任务完成后恢复中断，确保无论如何都恢复中断
                attachInterrupt(digitalPinToInterrupt(PIN_FINGERPRINT_TOUCH), handleTouchInterrupt, RISING);
                
                if (success) {
                    // 注册成功，生成默认的名字
                    String name_prefix = "指纹";
                    configManager.setFingerprintName(fingerprintId, name_prefix + String(fingerprintId + 1));
                    bluetoothManager.sendMessage(MSG_FINGERPRINT_REGISTER, &MSG_CMD_SUCCESS, 1);
                } else {
                    bluetoothManager.sendMessage(MSG_FINGERPRINT_REGISTER, &MSG_CMD_FAILURE, 1);
                }
                break;
            }
            case MSG_SET_FINGER_NAME: {
                Serial.println("[Task] Processing set fingerprint name");
                if (params->length < 2) {
                    Serial.println("[Task] Invalid set fingerprint name data");
                    bluetoothManager.sendMessage(MSG_SET_FINGER_NAME, &MSG_CMD_FAILURE, 1);
                    break;
                }
                int id = params->data[0];
                String name = String((char*)&params->data[1]);
                Serial.print("[Task] Setting fingerprint name for ID ");
                Serial.print(id);
                Serial.print(": ");
                Serial.println(name);
                if (configManager.setFingerprintName(id, name)) {
                    bluetoothManager.sendMessage(MSG_SET_FINGER_NAME, &MSG_CMD_SUCCESS, 1);
                } else {
                    bluetoothManager.sendMessage(MSG_SET_FINGER_NAME, &MSG_CMD_FAILURE, 1);
                }
                break;
            }
            case MSG_RENAME_FINGER_NAME: {
                // data: [id(1B)] + newName(utf8, 最多32B)
                if (params->length < 2) {
                    bluetoothManager.sendMessage(MSG_RENAME_FINGER_NAME, &MSG_CMD_FAILURE, 1);
                    break;
                }
                int id = params->data[0];
                String newName = String((char*)&params->data[1]);
                if (configManager.renameFingerprintName(id, newName)) {
                    bluetoothManager.sendMessage(MSG_RENAME_FINGER_NAME, &MSG_CMD_SUCCESS, 1);
                } else {
                    bluetoothManager.sendMessage(MSG_RENAME_FINGER_NAME, &MSG_CMD_FAILURE, 1);
                }
                break;
            }
            case MSG_FINGERPRINT_REGISTER_CANCEL:{
                Serial.println("[Task] Processing fingerprint registration cancel");
                bCancelRegister = true;
                bluetoothManager.sendMessage(MSG_FINGERPRINT_REGISTER_CANCEL, &MSG_CMD_SUCCESS, 1);
                break;
            }
            case MSG_FINGERPRINT_DELETE:{
                Serial.println("[Task] Processing delete fingerprint request");
                if (params->length < 2) {
                    Serial.println("[Task] Invalid delete fingerprint data");
                    bluetoothManager.sendMessage(MSG_FINGERPRINT_DELETE, &MSG_CMD_FAILURE, 1);
                    break;
                }
                int removeId = params->data[0];
                if (fingerprint.deleteFingerprint(removeId)) {
                    configManager.removeFingerprintName(removeId); // 同步删除名称
                    bluetoothManager.sendMessage(MSG_FINGERPRINT_DELETE, &MSG_CMD_SUCCESS, 1);
                } else {
                    bluetoothManager.sendMessage(MSG_FINGERPRINT_DELETE, &MSG_CMD_FAILURE, 1);
                }
                break;
            }
            case MSG_GET_INFO:{
                Serial.print("订阅完成耗时：");
                Serial.println(millis() - recordTime);
                Serial.println("[Task] Processing get info request");
                if(touchTriggered) {
                    xEventGroupSetBits(event_group, EVENT_BIT_BLE_NOTIFY);
                }
                MsgInfo info;
                info.sleepTime = configManager.getSleepTimeout();
                strncpy(info.deviceId, versionInfo.deviceId.c_str(), sizeof(info.deviceId) - 1);
                strncpy(info.buildDate, versionInfo.buildDate.c_str(), sizeof(info.buildDate) - 1);
                strncpy(info.firmwareVer, versionInfo.firmwareVersion.c_str(), sizeof(info.firmwareVer) - 1);
          
                Serial.println("[Task] MSG_GET_INFO return bluetoothMessage");
                bluetoothManager.sendMessage(MSG_GET_INFO, (uint8_t*)&info, sizeof(info));
                break;
            }
            case MSG_GET_FINGER_NAMES:{
                Serial.println("[Task] Processing get fingerprint names request");
                uint8_t indexTable[32] = {0};
                bool readIndexSuccess = fingerprint.readIndexTable(indexTable); // 读取索引表
                if (!readIndexSuccess) {
                    Serial.println("[Task] Failed to read fingerprint index table");
                    uint8_t errorMsg = 0xFF; // 用0xFF表示读取索引表失败
                    bluetoothManager.sendMessage(MSG_GET_FINGER_NAMES, &errorMsg, 1);
                    break;
                }
                
                std::vector<FPData> names;
                configManager.getAllFingerprintNames(names, indexTable);
                
                Serial.printf("[Task] Found %d fingerprint names\n", names.size());
                
                if(names.size() == 0) {
                    Serial.println("[Task] No fingerprint names found");
                    uint8_t buf[1] = {0}; // 返回一个字节表示没有指纹
                    if (!bluetoothManager.sendMessage(MSG_GET_FINGER_NAMES, buf, 1)) {
                        Serial.println("[Task] Failed to send empty fingerprint names response");
                    }
                } else {
                    uint8_t buf[1 + names.size() * sizeof(FPData)] = {0};
                    buf[0] = names.size(); // 第一个字节存储指纹数量
                    for (size_t i = 0; i < names.size(); ++i) {
                        memcpy((void*)&buf[1 + i * sizeof(FPData)], &names[i], sizeof(FPData));
                    }
                    Serial.println("[Task] MSG_GET_FINGER_NAMES return fingerprint names");
                    if (!bluetoothManager.sendMessage(MSG_GET_FINGER_NAMES, buf, 1 + names.size() * sizeof(FPData))) {
                        Serial.println("[Task] Failed to send fingerprint names response");
                    }
                }
                break;
            }
            case MSG_SET_SLEEPTIME:{
                Serial.println("[Task] Processing set sleep time request");
                if (params->length < sizeof(uint32_t)) {
                    Serial.println("[Task] Invalid sleep time data");
                    bluetoothManager.sendMessage(MSG_SET_SLEEPTIME, &MSG_CMD_FAILURE, 1);
                    break;
                }
                else
                {
                    uint32_t sleepTime;
                    memcpy(&sleepTime, params->data, sizeof(sleepTime));
                    configManager.setSleepTimeout(sleepTime);
                    configManager.save(); // 保存配置
                    bluetoothManager.sendMessage(MSG_SET_SLEEPTIME, &MSG_CMD_SUCCESS, 1);
                }
                break;
            }
            case MSG_REST_ALL:{
                Serial.println("[Task] Processing reset all request");
                // 恢复出厂设置
                configManager.clear();
                fingerprint.clearAllLib();
                bluetoothManager.sendMessage(MSG_REST_ALL, &MSG_CMD_SUCCESS, 1);
                break;
            }
            case MSG_LOCKSCREEN_STATUS:{
                Serial.println("[Task] Processing lock screen status request");
                xEventGroupSetBits(event_group, EVENT_BIT_SCREENLOCK); // 设置屏幕锁定事件位
                break;
            }
            case MSG_DEVICE_NOTIFY:{
                Serial.println("[Task] Recived PC connected notify");
                if(touchTriggered) {
                    Serial.println("[FP] 通知订阅通道已连接");
                    xEventGroupSetBits(event_group, EVENT_BIT_BLE_NOTIFY); // 设置设备通知事件位
                }
                bluetoothManager.isConnectedNotify = true;
                // 蓝牙连上了，恢复蓝色呼吸灯
                fingerprint.setLEDCmd(Fingerprint::LED_CODE_BREATH,0x01,0x01,0x00,18);  // 蓝色呼吸灯
                break;
            }
            case MSG_ENABLE_SLEEP:{
                Serial.println("[Task] Processing enable sleep request");
                if (params->length < 1) {
                    Serial.println("[Task] Invalid enable sleep data");
                    break;
                }
                bool enable = params->data[0] != 0; // 0表示禁用休眠，非0表示启用
                sleepManager.preventSleep(!enable);
                if (enable) {
                    Serial.println("[Task] Sleep mode enabled");
                } else {
                    Serial.println("[Task] Sleep mode disabled");
                }
                break;
            }
            case MSG_CHECK_SLEEP:{
                Serial.println("[Task] Processing check sleep request");
                if (params->length < 1) {
                    Serial.println("[Task] Invalid check sleep data");
                    xEventGroupSetBits(event_group, EVENT_BIT_BLE_SLEEP_ENABLE);
                    break;
                }
                bool bEnableSleep = params->data[0] != 0; // 0表示禁用休眠，非0表示启用
                if (bEnableSleep) {
                    Serial.println("[Task] Alow enter sleep mode");
                } else {
                    Serial.println("[Task] Deny enter sleep mode, UI actived");
                }
                xEventGroupSetBits(event_group, EVENT_BIT_BLE_SLEEP_ENABLE);
                break;
            }
            case MSG_FIRMWARE_UPDATE_START:{
                Serial.println("[Task] Processing firmware update >START<");
                if (params->length < 1) {
                    Serial.println("[Task] Invalid firmware update data");
                    bluetoothManager.sendMessage(MSG_FIRMWARE_UPDATE_START, &MSG_CMD_FAILURE, 1);
                    break;
                }
                // 固件更新开始，获取固件文件大小
                uint32_t total_size;
                memcpy(&total_size, params->data, sizeof(total_size));
                if (bluetoothOTA.begin(total_size)) {
                    Serial.println("[Task] Firmware update started");
                    bluetoothManager.sendMessage(MSG_FIRMWARE_UPDATE_START, &MSG_CMD_SUCCESS, 1);
                } else {
                    Serial.println("[Task] Firmware update failed");
                    bluetoothManager.sendMessage(MSG_FIRMWARE_UPDATE_START, &MSG_CMD_FAILURE, 1);
                }
                break;
            }
            case MSG_FIRMWARE_UPDATE_CHUNK:{
                Serial.println("[Task] Processing firmware update >CHUNK<");
                if (params->length < 1) {
                    Serial.println("[Task] Invalid firmware update data");
                    bluetoothManager.sendMessage(MSG_FIRMWARE_UPDATE_CHUNK, &MSG_CMD_FAILURE, 1);
                    break;
                }
                // 固件更新数据写入
                if (!bluetoothOTA.receiveData(params->data, params->length)) {
                    Serial.println("[Task] Failed to write firmware chunk data");
                    bluetoothManager.sendMessage(MSG_FIRMWARE_UPDATE_CHUNK, &MSG_CMD_FAILURE, 1);
                    break;
                }else{
                    Serial.println("[Task] Write firmware chunk success");
                    bluetoothManager.sendMessage(MSG_FIRMWARE_UPDATE_CHUNK, &MSG_CMD_SUCCESS, 1);
                }
                break;
            }
            case MSG_FIRMWARE_UPDATE_END:{
                Serial.println("[Task] Processing firmware update >END<");
                if (params->length < 1) {
                    Serial.println("[Task] Invalid firmware update end data");
                    bluetoothManager.sendMessage(MSG_FIRMWARE_UPDATE_END, &MSG_CMD_FAILURE, 1);
                    break;
                }
                // 固件更新结束，验证CRC32
                String targetCRC32 = String((char*)params->data);
                if (bluetoothOTA.finish(targetCRC32)) {
                    Serial.println("[Task] Firmware update completed successfully");
                    bluetoothManager.sendMessage(MSG_FIRMWARE_UPDATE_END, &MSG_CMD_SUCCESS, 1);
                    delay(1000);//等待蓝牙发送完毕后重启
                    ESP.restart();
                } else {
                    Serial.println("[Task] Firmware update failed or CRC32 mismatch");
                    bluetoothManager.sendMessage(MSG_FIRMWARE_UPDATE_END, &MSG_CMD_FAILURE, 1);
                }
                break;
            }
            default:{
                Serial.printf("[Task] Unknown message type: %02X\n", params->msgType);
                break;
            }
        }
    } catch (...) {
        Serial.println("[Task] Exception occurred while processing message");
    }
    
    // 确保params始终被释放
    delete params;
}

// 处理从蓝牙接收到的消息（改为入队）
void handleBluetoothMessage(uint8_t msgType, uint8_t* data, size_t length) {
    
    if (!bluetoothMsgQueue) {
        Serial.println("[BLE] Queue not initialized!");
        return;
    }
    
    TaskParameters* params = new TaskParameters();
    if (params == nullptr) {
        Serial.println("[BLE] Failed to allocate memory for TaskParameters");
        return;
    }
    
    params->msgType = msgType;
    memcpy(params->data, data, min(length, (size_t)MAX_DATA_LENGTH));
    params->length = length;

    //在这里单独判断取消类型的消息，不入队列了，因为之前队列中还等待取消在
    if(msgType == MSG_FINGERPRINT_REGISTER_CANCEL) {
        Serial.println("[BLE] Processing fingerprint register cancel immediately");
        bluetoothMessageTask(params);
        // 注意：bluetoothMessageTask会负责释放params
        return;
    }

    if (xQueueSend(bluetoothMsgQueue, &params, 0) != pdPASS) {
        Serial.println("[BLE] Queue full, message dropped!");
        delete params;
    } else {
        Serial.println("[BLE] Message enqueued");
    }
}