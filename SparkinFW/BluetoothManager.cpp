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
#include "SleepManager.h"
#include <esp_gap_ble_api.h>
extern ConfigManager configManager;
extern Fingerprint fingerprint; // 引入指纹模块对象
extern SleepManager sleepManager;

BluetoothManager::BluetoothManager()
{
    pServer = nullptr;
    pService = nullptr;
    pCharacteristic = nullptr;
    pBLE2902 = nullptr;
    pBleKeyboard = nullptr;
    
    deviceConnected = false;
    messageCallback = nullptr;
    pClientAddress = nullptr;
    autoReconnect = true;
    isAdvertising = false;
    isConnectedNotify = false;
    _autoAdvertisingEnabled = true; // 默认为true
    
    sendMutex = xSemaphoreCreateMutex();
    stateMutex = xSemaphoreCreateMutex(); // 初始化状态互斥锁
    _unpairRequest = false; // 初始化取消配对请求标志
}

void BluetoothManager::begin(const char *deviceName, BleKeyboard *bleKeyboard)
{
    // 保存BleKeyboard指针和设备名称
    pBleKeyboard = bleKeyboard;
    this->deviceName = String(deviceName);

    // 初始化BLE设备
    // 注意：Arduino-ESP32库会自动生成并保存BLE地址到NVS
    // 我们不需要手动设置地址，只在清除配对时才重新生成
    BLEDevice::init(deviceName);
    // 设置本地MTU
    BLEDevice::setMTU(251);

    // 设置安全参数 - 使用BLE绑定机制
    BLESecurity *pSecurity = new BLESecurity();
    pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND); // 启用绑定（配对）
    pSecurity->setCapability(ESP_IO_CAP_NONE);          // 无输入输出能力（Just Works配对）
    pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK); // 启用加密和身份识别密钥
    pSecurity->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK); // 响应端也启用
    
    // 启用 RPA
    //esp_ble_gap_config_local_privacy(true);

    // 打印当前已绑定设备数量
    int bondedCount = esp_ble_get_bond_device_num();
    Serial.printf("当前已绑定设备数量: %d\n", bondedCount);

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
    // 如果已经在广播中，直接返回，避免重复调用导致阻塞
    if (isAdvertising) {
        return;
    }
    
    // 开始广播
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();

    // 启动RPA 随机地址
    uint8_t dummy_addr[6];
    if(configManager.hasBLEAddress())
    {
        configManager.getBLEAddress(dummy_addr);
    }
    else
    {
        configManager.generateNewBLEAddress();
        configManager.getBLEAddress(dummy_addr);
    }
    Serial.print("BLE地址: ");
    for (int i = 0; i < 6; i++) {
        Serial.printf("%02X", dummy_addr[i]);
        if (i < 5) Serial.print(":");
    }
    Serial.println();
    pAdvertising->setDeviceAddress(dummy_addr, BLE_ADDR_TYPE_RANDOM);

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

    // 检查是否处于配对模式（无绑定设备=配对模式）
    int bondedCount = esp_ble_get_bond_device_num();
    if (bondedCount == 0)
    {
        Serial.println("配对模式：无绑定设备，允许新设备配对");
        // 设置LED灯为白色灯闪烁
        fingerprint.setLEDCmd(Fingerprint::LED_CODE_BLINK,0x07,(uint8_t)( (5 << 4) | 5 ), 0x00, 8);
    }
    else
    {
        Serial.printf("已有 %d 个绑定设备，只允许已绑定设备重连\n", bondedCount);
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

void BluetoothManager::loop()
{
    // 检查是否有取消配对的请求
    if (_unpairRequest) {
        Serial.println("[BluetoothManager] 检测到取消配对请求，开始执行...");
        _unpairRequest = false;
        unpairDevice();
        Serial.println("[BluetoothManager] 取消配对操作已完成");
    }

    // 如果禁用了自动广播，直接返回
    if (!_autoAdvertisingEnabled) {
        yield();
        return;
    }

    if(!deviceConnected && !isAdvertising)
    {
        // 如果设备未连接且未在广播中，需要进行广播让设备发现
        startAdvertising();
    }

    // 添加yield()调用，防止看门狗触发
    yield();
}

void BluetoothManager::requestUnpairDevice() {
    Serial.println("[BluetoothManager] 收到取消配对请求，将在主循环中执行");
    _unpairRequest = true;
}

void BluetoothManager::enableAutoAdvertising(bool enable) {
    _autoAdvertisingEnabled = enable;
    Serial.printf("自动广播已%s\n", enable ? "启用" : "禁用");
}

// 修改带参数的onConnect方法实现
void BluetoothManager::onConnect(BLEServer *pServer, esp_ble_gatts_cb_param_t *param)
{
    Serial.print("设备连接耗时：");
    Serial.println(millis() - recordTime);

    // 从param参数中获取客户端地址（仅用于日志显示）
    BLEAddress clientAddress(param->connect.remote_bda);
    Serial.print("[onConnect]客户端地址: ");
    Serial.println(clientAddress.toString().c_str());

    // 获取已绑定设备数量
    int bondedDevNum = esp_ble_get_bond_device_num();
    Serial.printf("[onConnect]当前已绑定设备数: %d\n", bondedDevNum);

    // 重要：一对一绑定模式
    // 如果已有绑定设备，且是新设备尝试连接，则拒绝
    if (bondedDevNum > 0) {
        // 已有绑定设备，只允许已绑定设备重连
        // ESP32底层会通过LTK自动校验，如果校验失败会自动断开
        // 这里不需要手动比对地址，ESP32会处理IRK解析
        Serial.println("[onConnect]已有绑定设备，等待ESP32底层LTK校验...");
        
        // 注意：在RPA场景下，这里看到的地址可能是随机地址
        // 真正的认证由ESP32底层完成，如果LTK校验失败会自动断开
        // 我们不需要也不应该手动比对地址
    } else {
        // 无绑定设备，处于配对模式，允许新设备配对
        Serial.println("[onConnect]配对模式：允许新设备配对");
    }

    // 允许连接，设置连接状态
    // 如果是未授权设备，ESP32底层会在后续的加密过程中拒绝
    if (stateMutex && xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
        deviceConnected = true;
        xSemaphoreGive(stateMutex);
    }
    
    Serial.println("[onConnect]客户端已连接，等待加密认证完成");

    // 通知BleKeyboard连接状态变化
    notifyKeyboardConnected();

    // 保存连接的客户端地址（仅用于显示，不用于认证）
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
    if(sleepManager.isSleepMode())
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
    // BLE外设模式不需要主动连接，等待中心设备（电脑）连接即可
    // 只需要检查是否有已绑定的设备
    int bondedDevNum = esp_ble_get_bond_device_num();
    
    if (bondedDevNum > 0) {
        Serial.printf("检测到 %d 个已绑定设备，等待其连接\n", bondedDevNum);
        
        // 打印已绑定设备列表
        esp_ble_bond_dev_t *bond_dev_list = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * bondedDevNum);
        if (bond_dev_list != nullptr) {
            esp_ble_get_bond_device_list(&bondedDevNum, bond_dev_list);
            for (int i = 0; i < bondedDevNum; i++) {
                BLEAddress bondedAddr(bond_dev_list[i].bd_addr);
                Serial.printf("已绑定设备 %d: %s\n", i, bondedAddr.toString().c_str());
            }
            free(bond_dev_list);
        }
        return true;
    }
    
    Serial.println("没有已绑定的设备");
    return false;
}

void BluetoothManager::clearPairedDevices()
{
    // 清除保存的配对信息（包括BLE底层绑定）
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
// 重新初始化BLE（用于更换地址）
void BluetoothManager::reinitBLE(const char* devName)
{
    Serial.println("[reinitBLE]开始重新初始化BLE");
    
    // 1. 停止广播
    if (isAdvertising) {
        stopAdvertising();
        delay(100);
    }
    
    // 2. 反初始化BLE
    Serial.println("[reinitBLE]反初始化BLE...");
    BLEDevice::deinit(true);  // 完全清理，包括NVS中的地址
    
    // 3. 重新初始化BLE
    // BLEDevice::init()会自动生成新的随机地址并保存到NVS
    Serial.println("[reinitBLE]重新初始化BLE");
    BLEDevice::init(devName);
    BLEDevice::setMTU(251);
    
    // 强制清理之前的pServer指针（如果还有残留），防止内存泄漏
    if (pServer != nullptr) {
        pServer = nullptr;
    }

    // 4. 重新设置安全参数
    Serial.println("[reinitBLE]设置安全参数...");
    BLESecurity *pSecurity = new BLESecurity();
    pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);
    pSecurity->setCapability(ESP_IO_CAP_NONE);
    pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    pSecurity->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    
    // 5. 重新创建服务器和服务
    Serial.println("[reinitBLE]创建Server...");
    pServer = BLEDevice::createServer();
    if (pServer != nullptr) {
        pServer->setCallbacks(this);
    } else {
        Serial.println("[reinitBLE] 创建 BLE Server 失败！");
        return;
    }
    
    Serial.println("[reinitBLE]创建Service...");
    pService = pServer->createService(SERVICE_UUID);
    
    Serial.println("[reinitBLE]创建Characteristic...");
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY |
        BLECharacteristic::PROPERTY_INDICATE);
    
    pBLE2902 = new BLE2902();
    pCharacteristic->addDescriptor(pBLE2902);
    pCharacteristic->setCallbacks(this);
    
    // 6. 重新初始化HID服务
    Serial.println("[reinitBLE]重启HID...");
    pBleKeyboard->begin(pServer);
    
    // 7. 启动服务
    Serial.println("[reinitBLE]启动Service...");
    pService->start();
    
    Serial.println("[reinitBLE]BLE重新初始化完成");
}

// 取消配对（清除配对信息、更换地址并重启广播）
void BluetoothManager::unpairDevice()
{
    Serial.println("取消配对：清除绑定信息、更换BLE地址并断开连接");
    // 禁止休眠，防止在取消配对过程中进入休眠
    sleepManager.preventSleep(true);
    // 0. 禁用自动广播，防止中途触发
    enableAutoAdvertising(false);

    // 1. 先断开当前连接
    if (isConnected()) {
        disconnectCurrentDevice();
        delay(100); // 等待断开完成
    }

    // 如果正在广播，也需要停止，以便后续重新生成地址并广播
    if (isAdvertising) {
        stopAdvertising();
        delay(100);
    }
    
    // 2. 清除所有BLE绑定
    clearPairedDevices();
    
    // 3. 重新初始化BLE
    // deinit(true)会清除BLE NVS数据，包括旧的地址
    // 下次init()时会自动生成新的随机地址
    //Serial.println("重新初始化BLE以更换地址...");
    //reinitBLE(deviceName.c_str());
    
    // 4. 清除后，自动进入配对模式（因为bondedCount=0）
    int bondedNum = esp_ble_get_bond_device_num();
    Serial.printf("配对已清除，当前绑定设备数: %d\n", bondedNum);
    Serial.println("已进入配对模式，可接受新设备配对");
    
    // 5. 设置LED为白色闪烁（配对模式指示）
    fingerprint.setLEDCmd(Fingerprint::LED_CODE_BLINK,0x07,(uint8_t)( (5 << 4) | 5 ), 0x00, 8);
    
    // 恢复自动广播（一定要在 startAdvertising 之前恢复，或者确保 startAdvertising 能工作）
    enableAutoAdvertising(true);

    // 6. 重新开始广播：会自动开始的，上面设置了自动广播。
    // delay(500);
    // startAdvertising();
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