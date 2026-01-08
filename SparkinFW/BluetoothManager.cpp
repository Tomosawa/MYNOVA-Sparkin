/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
#include "BluetoothManager.h"
#include "Common.h"
#include "ConfigManager.h"
#include "BluetoothHandle.h"
#include "Fingerprint.h"

extern ConfigManager configManager;
extern Fingerprint fingerprint; // 引入指纹模块对象

BluetoothManager::BluetoothManager()
{
    deviceConnected = false;
    messageCallback = nullptr;
    pClientAddress = nullptr;
    autoReconnect = true; // 默认启用自动重连
    isAdvertising = false;
    pBleKeyboard = nullptr;
    pServer = nullptr;
    pService = nullptr;
    pCharacteristic = nullptr;
    pBLE2902 = nullptr; // 初始化BLE2902指针
    isConnectedNotify = false;
    sendMutex = xSemaphoreCreateMutex();
    // 创建状态互斥锁
    stateMutex = xSemaphoreCreateMutex();
}

void BluetoothManager::begin(const char *deviceName, BleKeyboard *bleKeyboard)
{
    // 保存BleKeyboard指针
    pBleKeyboard = bleKeyboard;

    // 初始化BLE设备
    BLEDevice::init(deviceName);
    // 设置本地MTU
    BLEDevice::setMTU(251);

    // 设置安全参数
    BLESecurity *pSecurity = new BLESecurity();
    pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND); // 启用安全连接和绑定
    pSecurity->setCapability(ESP_IO_CAP_NONE);                 // 无输入输出能力
    pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

    // 创建BLE服务器
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(this);

    // 创建BLE服务
    pService = pServer->createService(SERVICE_UUID);

    // 创建BLE特征
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_NOTIFY |
            BLECharacteristic::PROPERTY_INDICATE);

    // 添加描述符并设置自定义回调
    pBLE2902 = new BLE2902();
    pCharacteristic->addDescriptor(pBLE2902);
    pCharacteristic->setCallbacks(this);

    // 初始化HID服务
    pBleKeyboard->begin(pServer);

    // 启动服务
    pService->start();

    // 初始化蓝牙消息处理队列
    initBluetoothMessageQueue();
   
}

long recordTime = 0; // 记录上次广播时间
// 添加新方法：开始广播
void BluetoothManager::startAdvertising(bool bFastMode)
{
    // 开始广播
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();

    // 添加HID服务UUID和自定义服务UUID
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->addServiceUUID(pBleKeyboard->getUUID());

    // 设置为键盘外观
    pAdvertising->setAppearance(HID_KEYBOARD); // 0x03C1 HID键盘外观值

    pAdvertising->setScanResponse(true);
    
    // pAdvertising->setMinPreferred(0x06); // 设置为iPhone连接的最小首选连接间隔
    // pAdvertising->setMinPreferred(0x12); // 设置为iPhone连接的最大首选连接间隔
    if(bFastMode)
    {
        pAdvertising->setMinInterval(0x0020); // 20 ms (0x0020 * 0.625 ms)
        pAdvertising->setMaxInterval(0x0030); // 100 ms (0x0064 * 0.625 ms)
        pAdvertising->setMinPreferred(8); //  10 ms (8 * 1.25 ms)
        pAdvertising->setMaxPreferred(16); // 20 ms (16 * 1.25 ms)
    }
    else
    { 
        pAdvertising->setMinInterval(0xA0); // 0xA0*0.625 = 100ms 最小广播间隔 
        pAdvertising->setMaxInterval(0x320); // 0x320*0.625 = 500ms 最大广播间隔 

        // 设置连接间隔
        pAdvertising->setMinPreferred(16); // 最小连接间隔 20ms
        pAdvertising->setMaxPreferred(32); // 最大连接间隔 40ms
    }
   
    recordTime = millis(); // 记录开始广播的时间
    BLEDevice::startAdvertising();

    isAdvertising = true;
    Serial.println("BLE设备已开始广播，等待客户端连接...");

    // 如果没有保存的设备说明在配对模式，设置LED灯
    String savedAddress;
    if (!configManager.getPairedDevice(savedAddress))
    {
        // 设置LED灯为白色灯闪烁
        fingerprint.setLEDCmd(Fingerprint::LED_CODE_BLINK,0x07,(uint8_t)( (5 << 4) | 5 ), 0x00, 8);
    }
}

// 添加新方法：停止广播
void BluetoothManager::stopAdvertising()
{
    BLEDevice::stopAdvertising();
    isAdvertising = false;
    Serial.println("BLE设备已停止广播");
}

void BluetoothManager::setMessageCallback(MessageCallback callback)
{
    messageCallback = callback;
}

bool BluetoothManager::sendMessage(const uint8_t msgType, const uint8_t *data, size_t length)
{
    if (!deviceConnected || pCharacteristic == nullptr)
    {
        return false;
    }

    if (xSemaphoreTake(sendMutex, portMAX_DELAY) == pdTRUE) {
        // 创建消息缓冲区：消息类型(1字节) + 数据长度(1字节) + 数据
        uint8_t *buffer = new uint8_t[length + 3];
        if (buffer == nullptr)
        {
            Serial.println("内存分配失败");
            xSemaphoreGive(sendMutex);
            return false;
        }

        buffer[0] = msgType;
        // 用2字节存储长度，兼容更大数据包
        buffer[1] = (length >> 8) & 0xFF;
        buffer[2] = length & 0xFF;

        if (data != nullptr && length > 0)
        {
            memcpy(buffer + 3, data, length);
        }

        // 发送数据
        pCharacteristic->setValue(buffer, length + 3);
        pCharacteristic->notify();

        delete[] buffer;
        delay(50);
        xSemaphoreGive(sendMutex);
        return true;
    }
    return false;
}

void BluetoothManager::update()
{
    if(!deviceConnected && !isAdvertising)
    {
        // 如果设备未连接且未在广播中，开始广播
        startAdvertising();
    }
   
    // // 处理连接状态变化
    // if (!deviceConnected && oldDeviceConnected)
    // {
    //     delay(500); // 给蓝牙堆栈时间处理断开连接
    //     oldDeviceConnected = deviceConnected;

    //     // 如果有已保存的设备，尝试重新连接
    //     String savedAddress;
    //     if (autoReconnect && configManager.getPairedDevice(savedAddress))
    //     {
    //         isConnectingToPaired = true;
    //         Serial.println("设备断开连接，将尝试重新连接到已保存的设备");
    //     }
    //     else
    //     {
    //         // 没有已保存的设备，重新开始广播
    //         // 添加延迟防止蓝牙堆栈不稳定
    //         delay(1000);
    //         startAdvertising();
    //     }
    // }

    // // 处理新连接
    // if (deviceConnected && !oldDeviceConnected)
    // {
    //     oldDeviceConnected = deviceConnected;
    //     //Serial.println("设备已连接");
    //     isConnectingToPaired = false;
    // }

    // 如果正在尝试连接到已保存的设备，持续尝试重连
    // if (isConnectingToPaired)
    // {
    //     static unsigned long lastReconnectAttempt = 0;
    //     static int reconnectAttempts = 0;
    //     unsigned long currentTime = millis();

    //     // 每5秒尝试重连一次，但增加失败次数限制
    //     if (currentTime - lastReconnectAttempt > 5000)
    //     {
    //         lastReconnectAttempt = currentTime;

    //         // 增加重连次数限制，防止无限循环
    //         if (reconnectAttempts < 5)
    //         {
    //             bool connected = connectToPairedDevice();
    //             reconnectAttempts++;

    //             if (connected)
    //             {
    //                 reconnectAttempts = 0; // 重置计数器
    //                 isConnectingToPaired = false;
    //             }
    //         }
    //         else
    //         {
    //             // 多次重连失败，不清除配对信息，只停止重试并关闭自动重连
    //             Serial.println("多次重连失败，停止重试。配对信息已保留，可能是设备已关机或不在范围内");
    //             isConnectingToPaired = false;
    //             reconnectAttempts = 0; // 重置计数器
    //             autoReconnect = false; // 关闭自动重连
    //         }
    //     }

    //     // 防止阻塞主循环
    //     yield();
    // }

    // 添加yield()调用，防止看门狗触发
    yield();
}

// 修改带参数的onConnect方法实现
void BluetoothManager::onConnect(BLEServer *pServer, esp_ble_gatts_cb_param_t *param)
{
    Serial.print("设备连接耗时：");
    Serial.println(millis() - recordTime);

    // 从param参数中获取客户端地址
    BLEAddress clientAddress(param->connect.remote_bda);

    Serial.print("[onConnect]客户端地址: ");
    Serial.println(clientAddress.toString().c_str());

    // 检查是否有已保存的配对设备
    String savedAddress;
    if (configManager.getPairedDevice(savedAddress))
    {
        // 如果有已配对设备，检查连接的设备是否为已配对设备
        if (clientAddress.toString() != savedAddress)
        {
            // 不是已配对设备，拒绝连接
            Serial.println("[onConnect]非配对设备尝试连接，拒绝连接");
            pServer->disconnect(param->connect.conn_id);
            return;
        }
    }
    else
    {
        // 没有已配对设备，允许连接
        Serial.println("[onConnect]没有已配对设备，允许新的配对");
        configManager.setPairedDevice(clientAddress.toString());
        configManager.save();
    }

    // 是已配对设备或者还没有配对过任何设备，允许连接
    // 使用互斥锁保护状态变量
    if (stateMutex && xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
        deviceConnected = true;
        xSemaphoreGive(stateMutex);
    }
    
    Serial.println("[onConnect]客户端已连接");

    // 通知BleKeyboard连接状态变化
    notifyKeyboardConnected();

    // 保存连接的客户端地址
    if (stateMutex && xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
        if (pClientAddress != nullptr)
        {
            delete pClientAddress;
        }
        pClientAddress = new BLEAddress(param->connect.remote_bda);
        xSemaphoreGive(stateMutex);
    }

    // 连接成功后停止广播，不允许其他设备连接
    if (stateMutex && xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
        if (isAdvertising)
        {
            // 设置灯为正常
            fingerprint.setLEDCmd(Fingerprint::LED_CODE_BREATH,0x01,0x01,0x00);
            // 系统默认会停止不需要手动停止，只需要设置标记
            isAdvertising = false;
            if(touchTriggered)
            {
                xEventGroupSetBits(event_group, EVENT_BIT_BLE_CONNECTED);
            }
        }
        xSemaphoreGive(stateMutex);
    }
}

void BluetoothManager::onDisconnect(BLEServer *pServer)
{
    // 使用互斥锁保护状态变量
    bool wasConnected = false;
    if (stateMutex && xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
        // 保存先前的连接状态
        wasConnected = deviceConnected;
        deviceConnected = false;
        xSemaphoreGive(stateMutex);
    }

    Serial.println("[onDisconnect]客户端已断开连接");

    // 先清除客户端地址，防止后续访问野指针
    if (stateMutex && xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
        if (pClientAddress != nullptr)
        {
            delete pClientAddress;
            pClientAddress = nullptr;
        }
        xSemaphoreGive(stateMutex);
    }

    // 如果是睡眠模式要求断开，则发送断开连接事件
    if(bSleepMode)
        xEventGroupSetBits(event_group, EVENT_BIT_BLE_DISCONNECTED);

    // 安全处理断开连接，避免 BleKeyboard 内部处理引起的崩溃
    if (wasConnected)
    {
        // 使用延迟和安全处理
        delay(200);

        // 不再调用可能导致崩溃的 notifyKeyboardDisconnected
        // 而是直接更新 pBleKeyboard 的状态
        if (pBleKeyboard != nullptr)
        {
            pBleKeyboard->connected = false;
        }
        
        // 再次延迟给蓝牙堆栈时间
        delay(300);
    }

    // 清除事件位
    if (stateMutex && xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
        xEventGroupClearBits(event_group, EVENT_BIT_BLE_NOTIFY);
        isConnectedNotify = false;
        xSemaphoreGive(stateMutex);
    }
    
    // 手动调用 yield() 确保系统有机会处理其他任务
    yield();
}

void BluetoothManager::onWrite(BLECharacteristic *pCharacteristic)
{
    if (pCharacteristic == nullptr)
    {
        return;
    }

    String value = pCharacteristic->getValue();

    if (value.length() > 0 && messageCallback != nullptr)
    {
        uint8_t msgType = value[0];
        uint8_t *data = nullptr;
        size_t length = 0;

        if (value.length() > 1)
        {
            data = (uint8_t *)(value.c_str() + 1);
            length = value.length() - 1;
        }

        // 调用回调函数处理其他消息
        messageCallback(msgType, data, length);
    }
}

bool BluetoothManager::connectToPairedDevice()
{
    String savedAddress;
    if (!configManager.getPairedDevice(savedAddress))
    {
        return false;
    }

    Serial.print("尝试连接到已保存的设备: ");
    Serial.println(savedAddress);

    BLEAddress bleAddress(savedAddress);
    BLEClient *pClient = BLEDevice::createClient();
 
    // 添加超时机制，防止连接操作阻塞太久
    unsigned long startTime = millis();
    bool connected = false;

    try
    {
        connected = pClient->connect(bleAddress);
    }
    catch (...)
    {
        Serial.println("连接过程中发生异常");
        connected = false;
    }

    if (connected)
    {
        Serial.println("成功连接到已保存的设备!");
        deviceConnected = true;
        return true;
    }
    else
    {
        Serial.println("无法连接到已保存的设备，等待设备连接...");
        delete pClient;
        return false;
    }
}

bool BluetoothManager::pairDevice()
{
    if (!isConnected() || pClientAddress == nullptr)
    {
        return false;
    }

    // 保存当前连接的设备地址
    configManager.setPairedDevice(pClientAddress->toString());
    configManager.save();
    return true;
}

void BluetoothManager::clearPairedDevices()
{
    // 清除保存的配对信息
    configManager.clearPairedDevices();
    Serial.println("已清除所有配对设备信息");
}

bool BluetoothManager::checkAndConnect()
{
    if (!isConnected())
    {
        return false;//connectToPairedDevice();
    }
    else
    {
        return true;
    }
}

void BluetoothManager::notifyKeyboardConnected()
{
    if (pBleKeyboard != nullptr && pServer != nullptr)
    {
        // 调用BleKeyboard的onConnect方法
        pBleKeyboard->onConnect(pServer);
    }
}

void BluetoothManager::notifyKeyboardDisconnected()
{
    // 断开连接时使用 try-catch 块来捕获可能的异常
    if (pBleKeyboard != nullptr && pServer != nullptr)
    {
        try
        {
            // 调用 BleKeyboard 的 onDisconnect 方法前先设置一个标志
            bool wasConnected = pBleKeyboard->isConnected();

            // 在调用回调之前，临时禁用 BleKeyboard 的广播功能，避免触发崩溃
            pBleKeyboard->connected = false;

            // 只有在之前是连接状态时才调用 onDisconnect
            if (wasConnected)
            {
                // 直接修改 BleKeyboard 内部状态，而不是通过回调
                pBleKeyboard->connected = false;
            }
        }
        catch (...)
        {
            Serial.println("处理键盘断开连接时发生异常");
        }
    }
}

// 主动断开当前蓝牙连接
void BluetoothManager::disconnectCurrentDevice()
{
    if (deviceConnected && pServer != nullptr && pClientAddress != nullptr)
    {
        Serial.println("主动断开当前蓝牙连接");
        pServer->disconnect(0); // 0为默认conn_id，实际可遍历连接id

        // std::map<uint16_t, conn_status_t> devices = pServer->getPeerDevices(true);
        // // 遍历客户端设备并打印信息
        // for (auto const& pair : devices) {
        //     uint16_t connId = pair.first;
        //     pServer->removePeerDevice(connId, true); // 移除连接状态
        //     Serial.printf("取消配对连接ID: %d, \n", connId);
        // }
        isConnectedNotify = false;
        deviceConnected = false;
        // 断开后自动触发 onDisconnect 回调
    }
}
// 取消配对（清除配对信息并重启广播）
void BluetoothManager::unpairDevice()
{
    Serial.println("清除配对信息并断开蓝牙连接");
     // 清除ESP32底层的绑定信息
    int dev_num = esp_ble_get_bond_device_num();
    if (dev_num > 0) {
        esp_ble_bond_dev_t *dev_list = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
        esp_ble_get_bond_device_list(&dev_num, dev_list);
        for (int i = 0; i < dev_num; i++) {
            esp_ble_remove_bond_device(dev_list[i].bd_addr);
        }
        free(dev_list);
        Serial.println("已清除ESP32底层蓝牙绑定信息");
    }
    
    clearPairedDevices(); // 清除配对信息
    disconnectCurrentDevice(); // 主动断开
}

bool BluetoothManager::isNotificationEnabled() {
    return pBLE2902 && pBLE2902->getNotifications() && isConnectedNotify;
}

bool BluetoothManager::isConnected() {
    bool connected = false;
    if (stateMutex && xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
        connected = pServer && pServer->getConnectedCount() > 0;
        xSemaphoreGive(stateMutex);
    }
    return connected;
}

void BluetoothManager::setBatteryLevel(uint8_t level) {
    if (pBleKeyboard != nullptr) {
        pBleKeyboard->setBatteryLevel(level);
    }
    Serial.printf("设置电池电量: %d%%\n", level);
}