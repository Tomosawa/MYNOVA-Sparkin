using System;
using System.Text;
using System.Threading.Tasks;
using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Devices.Enumeration;
using Windows.Storage.Streams;
using System.Linq;
using System.Threading;
using NLog;
using System.Collections.Generic;
using System.Net;
using System.Collections.Concurrent;
/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
namespace SparkinLib.Bluetooth
{
    public class BluetoothManager
    {
        // 服务和特征值UUID常量
        public static readonly Guid DEFAULT_SERVICE_UUID = new Guid("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
        public static readonly Guid DEFAULT_CHARACTERISTIC_UUID = new Guid("beb5483e-36e1-4688-b7f5-ea07361b26a8");
        public static readonly string BLUETOOTH_DEVICE_NAME = "Sparkin FP01";

        // 电量服务和特征值UUID
        private static readonly Guid BATTERY_SERVICE_UUID = new Guid("0000180F-0000-1000-8000-00805F9B34FB");
        private static readonly Guid BATTERY_CHARACTERISTIC_UUID = new Guid("00002A19-0000-1000-8000-00805F9B34FB");

        // 设备连接监听相关
        private DeviceWatcher deviceWatcher;        // 设备监听器
        private bool isMonitoringPairedDevices;     // 是否正在监听配对消息

        // 连接相关
        private BluetoothLEDevice connectedDevice = null;       //已连接的设备（只有一个）
        //private DeviceInformation connectedDeviceInfo = null;   //已连接的设备信息
        private GattDeviceService selectedService;  //订阅的通讯服务
        private GattCharacteristic selectedCharacteristic;

        // 事件
        public event EventHandler<BluetoothDeviceInfo> DeviceConnected;
        public event EventHandler<BluetoothDeviceInfo> DeviceDisconnected;
        public event EventHandler<byte[]> DataReceived;
        public event EventHandler<string> ErrorOccurred;
        public event EventHandler<string> DeviceRemoved;

        // 蓝牙设备清单
        private ConcurrentDictionary<string, BluetoothDeviceInfo> bluetoothDevices = new ConcurrentDictionary<string, BluetoothDeviceInfo>();
        private ConcurrentDictionary<string, BluetoothLEDevice> bleDevices = new ConcurrentDictionary<string, BluetoothLEDevice>();
        // 日志记录
        private Logger log = LogUtil.GetLogger();

        public BluetoothManager()
        {
            isMonitoringPairedDevices = false;
        }

        #region 监听服务控制
        /// <summary>
        /// 开始监听已配对设备的连接
        /// </summary>
        public void StartMonitoringPairedDevices()
        {
            try
            {
                if (isMonitoringPairedDevices)
                {
                    StopMonitoringPairedDevices();
                }

                // 创建配对设备监视器
                // AQS 查询字符串，用于监视所有蓝牙LE设备
                string bleSelector = BluetoothLEDevice.GetDeviceSelectorFromDeviceName(BLUETOOTH_DEVICE_NAME);

                log.Info("[BTM_START]开始监听已配对的蓝牙设备");

                // 创建设备监视器
                deviceWatcher = DeviceInformation.CreateWatcher(
                    bleSelector,
                    new string[] { "System.Devices.Aep.DeviceAddress", "System.Devices.Aep.IsConnected", "System.Devices.Aep.Bluetooth.Le.IsConnectable", "System.Devices.Aep.SignalStrength", "System.Devices.Aep.IsPresent" },
                    DeviceInformationKind.AssociationEndpoint);

                // 注册事件处理程序
                deviceWatcher.Added += DeviceWatcher_Added;
                deviceWatcher.Updated += DeviceWatcher_Updated;
                deviceWatcher.Removed += DeviceWatcher_Removed;

                // 启动监视器
                deviceWatcher.Start();
                isMonitoringPairedDevices = true;
            }
            catch (UnauthorizedAccessException ex)
            {
                log.Info($"[BTM_START]无权访问蓝牙设备: {ex.Message}");
                ErrorOccurred?.Invoke(this, "无法访问蓝牙设备，请确认应用有足够的权限");
            }
            catch (Exception ex)
            {
                log.Info($"[BTM_START]开始监听已配对设备时出错: {ex.Message}");
                ErrorOccurred?.Invoke(this, $"开始监听已配对设备时出错: {ex.Message}");
            }
        }

        /// <summary>
        /// 停止监听已配对设备的连接
        /// </summary>
        public void StopMonitoringPairedDevices()
        {
            try
            {
                if (deviceWatcher != null)
                {
                    // 停止监视器
                    if (deviceWatcher.Status == DeviceWatcherStatus.Started ||
                        deviceWatcher.Status == DeviceWatcherStatus.EnumerationCompleted)
                    {
                        deviceWatcher.Stop();
                    }

                    // 取消事件注册
                    deviceWatcher.Added -= DeviceWatcher_Added;
                    deviceWatcher.Removed -= DeviceWatcher_Removed;

                    deviceWatcher = null;
                    isMonitoringPairedDevices = false;

                    log.Info("[BTM_STOP]停止监听已配对蓝牙设备");
                }
            }
            catch (Exception ex)
            {
                log.Info($"[BTM_STOP]停止监听已配对设备时出错: {ex.Message}");
                ErrorOccurred?.Invoke(this, $"停止监听已配对设备时出错: {ex.Message}");
            }
        }
        #endregion

        #region 蓝牙配对和连接
        /// <summary>
        /// 断开当前连接的蓝牙设备
        /// </summary>
        public void DisconnectAsync()
        {
            try
            {
                // 创建本地变量引用设备，避免多线程访问问题
                var localCharacteristic = selectedCharacteristic;
                var localService = selectedService;

                // 先清空引用，使UI代码可以立即检测到断开状态
                selectedCharacteristic = null;
                selectedService = null;

                // 直接执行资源释放，不使用后台任务
                try
                {
                    // 关闭特征值通知
                    if (localCharacteristic != null)
                    {
                        try
                        {
                            if (localCharacteristic.CharacteristicProperties.HasFlag(GattCharacteristicProperties.Notify) ||
                                localCharacteristic.CharacteristicProperties.HasFlag(GattCharacteristicProperties.Indicate))
                            {
                                // 取消事件注册
                                localCharacteristic.ValueChanged -= Characteristic_ValueChanged;
                                localCharacteristic = null;
                            }
                        }
                        catch (Exception ex)
                        {
                            log.Info($"[BTM_Disconnect]取消特征值通知时出错: {ex.Message}");
                        }
                    }

                    // 关闭服务
                    if (localService != null)
                    {
                        localService.Dispose();
                    }
                }
                catch (Exception ex)
                {
                    log.Info($"[BTM_Disconnect]释放蓝牙资源时出错: {ex.Message}");
                }

                log.Info("[BTM_Disconnect]已断开设备连接");
            }
            catch (Exception ex)
            {
                log.Info($"[BTM_Disconnect]断开连接时出错: {ex.Message}");
                ErrorOccurred?.Invoke(this, $"断开连接时出错: {ex.Message}");
            }
        }

        /// <summary>
        /// 取消与设备的配对
        /// </summary>
        /// <param name="deviceId">设备ID</param>
        /// <returns>取消配对结果</returns>
        public async Task<bool> UnpairDeviceAsync(string deviceId)
        {
            try
            {
                // 获取设备信息
                DeviceInformation deviceInfo = await DeviceInformation.CreateFromIdAsync(deviceId);
                if (deviceInfo == null)
                {
                    log.Info("[BTM_UnpairDevice]无法获取设备信息");
                    ErrorOccurred?.Invoke(this, "无法获取设备信息");
                    return false;
                }

                // 检查是否已配对
                if (!deviceInfo.Pairing.IsPaired)
                {
                    log.Info("[BTM_UnpairDevice]设备未配对");
                    return true;
                }

                // 取消配对
                DeviceUnpairingResult unpairingResult = await deviceInfo.Pairing.UnpairAsync();
                if (unpairingResult.Status == DeviceUnpairingResultStatus.Unpaired)
                {
                    log.Info("[BTM_UnpairDevice]取消配对成功");
                    return true;
                }
                else
                {
                    log.Info($"[BTM_UnpairDevice]取消配对失败: {unpairingResult.Status}");
                    ErrorOccurred?.Invoke(this, $"取消配对失败: {unpairingResult.Status}");
                    return false;
                }
            }
            catch (Exception ex)
            {
                log.Info($"[BTM_UnpairDevice]取消配对设备时出错: {ex.Message}");
                ErrorOccurred?.Invoke(this, $"取消配对设备时出错: {ex.Message}");
                return false;
            }
        }

        public Task<DeviceInformation> FastSearchDeviceByNameAsync(string deviceName, int timeoutMs = 3000)
        {
            var tcs = new TaskCompletionSource<DeviceInformation>();
            string selector = BluetoothLEDevice.GetDeviceSelectorFromDeviceName(deviceName);

            DeviceWatcher watcher = DeviceInformation.CreateWatcher(
                selector,
                new string[] { "System.Devices.Aep.DeviceAddress", "System.Devices.Aep.IsConnected", "System.Devices.Aep.Bluetooth.Le.IsConnectable", "System.Devices.Aep.SignalStrength", "System.Devices.Aep.IsPresent" },
                    DeviceInformationKind.AssociationEndpoint
            );

            // 设备找到时触发
            watcher.Added += (sender, deviceInfo) =>
            {
                log.Info($"[FastSearch]找到设备: {deviceInfo.Name}");
                tcs.TrySetResult(deviceInfo);
                watcher.Stop(); // 找到后立即停止搜索
            };

            // 搜索超时处理
            var timeoutTokenSource = new CancellationTokenSource();
            timeoutTokenSource.Token.Register(() =>
            {
                if (!tcs.Task.IsCompleted)
                {
                    log.Info($"[FastSearch]搜索超时，未找到设备: {deviceName}");
                    tcs.TrySetResult(null);
                    watcher.Stop();
                }
            });

            // 启动搜索和超时计时器
            watcher.Start();
            timeoutTokenSource.CancelAfter(timeoutMs);

            return tcs.Task;
        }

        /// <summary>
        /// UI中直接进行蓝牙配对
        /// </summary>
        public async Task<bool> UIPairDeviceAsync(string deviceId)
        {
            try
            {
                log.Info("[UI_BT_PAIR]通过ID获取设备信息...");

                // 获取第一个找到的设备
                DeviceInformation deviceInfo = await DeviceInformation.CreateFromIdAsync(deviceId);
                log.Info($"[UI_BT_PAIR]得到了设备信息: {deviceInfo.Name}, ID: {deviceInfo.Id}");

                // 进行配对
                if (!deviceInfo.Pairing.IsPaired && deviceInfo.Pairing.CanPair)
                {
                    log.Info("[UI_BT_PAIR]设备未配对，开始配对过程...");

                    // 配置配对选项 - 优先使用不需要用户交互的配对方式
                    DevicePairingKinds ceremonySelection = DevicePairingKinds.None;
                    ceremonySelection |= DevicePairingKinds.ConfirmOnly;  // 首选，只需确认

                    // 添加其他配对方式以支持所有可能的配对方式
                    ceremonySelection |= DevicePairingKinds.ProvidePin;
                    ceremonySelection |= DevicePairingKinds.DisplayPin;
                    ceremonySelection |= DevicePairingKinds.ConfirmPinMatch;
                    ceremonySelection |= DevicePairingKinds.ProvidePasswordCredential;

                    log.Info($"[UI_BT_PAIR]配对选项已配置: {ceremonySelection}");

                    // 创建自定义配对
                    DeviceInformationCustomPairing customPairing = deviceInfo.Pairing.Custom;
                    log.Info("[UI_BT_PAIR]已创建自定义配对对象");

                    // 注册配对请求事件
                    customPairing.PairingRequested += PairingRequested;
                    log.Info("[UI_UI_BT_PAIRBT]已注册PairingRequested事件");

                    try
                    {
                        log.Info("[UI_BT_PAIR]开始配对...");

                        // 请求配对
                        log.Info($"[UI_BT_PAIR]调用PairAsync，ceremonySelection={ceremonySelection}");
                        DevicePairingResult pairingResult = await customPairing.PairAsync(
                            ceremonySelection,
                            DevicePairingProtectionLevel.None);  // 使用较低的保护级别

                        if (pairingResult.Status == DevicePairingResultStatus.Paired ||
                            pairingResult.Status == DevicePairingResultStatus.AlreadyPaired)
                        {
                            log.Info($"[UI_BT_PAIR]配对成功: {pairingResult.Status}");
                            return true;
                        }
                        else
                        {
                            log.Error($"[UI_BT_PAIR]配对失败: {pairingResult.Status}");
                            return false;
                        }
                    }
                    catch (UnauthorizedAccessException ex)
                    {
                        log.Error($"[UI_BT_PAIR]权限不足: {ex.Message}");
                        ErrorOccurred?.Invoke(this, "权限不足，请确认设备可用");
                    }
                    catch (InvalidOperationException ex)
                    {
                        log.Error($"[UI_BT_PAIR]操作无效: {ex.Message}");
                        ErrorOccurred?.Invoke(this, "操作无效，请检查设备状态");
                    }
                    finally
                    {
                        // 清理事件处理
                        customPairing.PairingRequested -= PairingRequested;
                    }
                }
                else
                {
                    log.Info("[UI_BT_PAIR]设备已经配对");
                }

                return false;
            }
            catch (Exception ex)
            {
                log.Error($"[UI_BT_PAIR]配对设备时出错: {ex.Message}");
                log.Error($"[UI_BT_PAIR]异常堆栈: {ex.StackTrace}");
                return false;
            }
        }

        /// <summary>
        /// 异步取消配对指定的蓝牙设备。
        /// </summary>
        /// <param name="deviceId">要取消配对的设备ID。</param>
        /// <returns>如果取消配对成功或设备本就未配对，则返回 true；否则返回 false。</returns>
        public async Task<bool> UIUnpairDeviceAsync(string deviceId)
        {
            try
            {
                log.Info("[UI_BT_UNPAIR]通过ID获取设备信息...");

                // 获取第一个找到的设备
                DeviceInformation deviceInfo = await DeviceInformation.CreateFromIdAsync(deviceId);
                log.Info($"[UI_BT_UNPAIR]得到了设备信息: {deviceInfo.Name}, ID: {deviceInfo.Id}");

                // 检查设备是否已配对
                if (deviceInfo.Pairing.IsPaired)
                {
                    log.Info("[UI_BT_UNPAIR]设备已配对，开始取消配对过程...");

                    try
                    {
                        log.Info("[UI_BT_UNPAIR]调用UnpairAsync...");

                        // 执行取消配对操作
                        DeviceUnpairingResult unpairingResult = await deviceInfo.Pairing.UnpairAsync();

                        if (unpairingResult.Status == DeviceUnpairingResultStatus.Unpaired)
                        {
                            log.Info($"[UI_BT_UNPAIR]取消配对成功: {unpairingResult.Status}");
                            return true;
                        }
                        else
                        {
                            // UnpairAsync 通常只返回 Unpaired 或 Failed
                            log.Error($"[UI_BT_UNPAIR]取消配对失败: {unpairingResult.Status}");
                            // 可以根据需要触发错误事件
                            ErrorOccurred?.Invoke(this, $"取消配对设备 '{deviceInfo.Name}' 失败。");
                            return false;
                        }
                    }
                    catch (UnauthorizedAccessException ex)
                    {
                        log.Error($"[UI_BT_UNPAIR]权限不足: {ex.Message}");
                        ErrorOccurred?.Invoke(this, "权限不足，无法取消配对设备。");
                    }
                    catch (Exception ex) // 捕获其他可能的异常，如设备已断开连接等
                    {
                        log.Error($"[UI_BT_UNPAIR]取消配对时发生异常: {ex.Message}");
                        log.Error($"[UI_BT_UNPAIR]异常堆栈: {ex.StackTrace}");
                        ErrorOccurred?.Invoke(this, $"取消配对设备 '{deviceInfo.Name}' 时发生错误: {ex.Message}");
                    }
                }
                else
                {
                    log.Info("[UI_BT_UNPAIR]设备未配对，无需操作。");
                    return true;
                }

                return false;
            }
            catch (Exception ex)
            {
                // 这里的异常通常是 CreateFromIdAsync 失败，例如设备ID无效或设备不存在
                log.Error($"[UI_BT_UNPAIR]获取设备信息时出错: {ex.Message}");
                log.Error($"[UI_BT_UNPAIR]异常堆栈: {ex.StackTrace}");

                ErrorOccurred?.Invoke(this, "找不到指定的设备。");
                return false;
            }
        }

        /// <summary>
        /// 配对请求事件处理
        /// </summary>
        private void PairingRequested(DeviceInformationCustomPairing sender, DevicePairingRequestedEventArgs args)
        {
            try
            {
                log.Info($"[BTM_PairingRequested]收到配对请求，配对类型: {args.PairingKind}");
                
                switch (args.PairingKind)
                {
                    case DevicePairingKinds.ConfirmOnly:
                        // 只需用户确认配对
                        log.Info("[BTM_PairingRequested]收到ConfirmOnly配对请求，自动接受");
                        args.Accept();
                        break;

                    case DevicePairingKinds.DisplayPin:
                        // 显示PIN码
                        log.Info($"[BTM_PairingRequested]收到DisplayPin配对请求，PIN码: {args.Pin}");
                        args.Accept();
                        break;

                    case DevicePairingKinds.ProvidePin:
                        // 需要提供PIN码
                        log.Info("[BTM_PairingRequested]收到ProvidePin配对请求，提供默认PIN码0000");
                        args.Accept("0000"); // 提供默认PIN码
                        break;

                    case DevicePairingKinds.ConfirmPinMatch:
                        // 需要确认PIN码匹配
                        log.Info($"[BTM_PairingRequested]收到ConfirmPinMatch配对请求，PIN码: {args.Pin}");
                        args.Accept();
                        break;

                    default:
                        log.Info($"[BTM_PairingRequested]收到不支持的配对类型: {args.PairingKind}");
                        args.Accept(); // 尝试接受其他类型的配对请求
                        break;
                }
                
                log.Info("[BTM_PairingRequested]已处理配对请求");
            }
            catch (Exception ex)
            {
                log.Info($"[BTM_PairingRequested]处理配对请求时出错: {ex.Message}");
                log.Info($"[BTM_PairingRequested]异常堆栈: {ex.StackTrace}");
            }
        }

        /// <summary>
        /// 连接成功后自动订阅默认服务和特征值
        /// </summary>
        /// <returns>订阅是否成功</returns>
        private async Task<bool> AutoSubscribeToServiceAsync()
        {
            try
            {
                if (connectedDevice == null)
                {
                    log.Info("[BTM_AutoSubScr]未连接到设备，无法订阅服务");
                    return false;
                }

                log.Info("[BTM_AutoSubScr]尝试自动订阅默认服务和特征值");

                // 尝试订阅默认服务和特征值
                bool result = false;

                try
                {
                    // 再次检查连接状态，以防在此期间设备已断开
                    if (connectedDevice == null || connectedDevice.ConnectionStatus != BluetoothConnectionStatus.Connected)
                    {
                        log.Info("[BTM_AutoSubScr]设备已断开连接或连接状态异常，中止订阅流程");
                        return false;
                    }
                    if (selectedCharacteristic != null)
                    {
                        log.Info("[BTM_AutoSubScr]已经订阅了，无需重复订阅！");
                        return true;
                    }

                    // 订阅指定特征和服务
                    result = await SelectServiceAndCharacteristicAsync(DEFAULT_SERVICE_UUID, DEFAULT_CHARACTERISTIC_UUID);
                }
                catch (Exception ex)
                {
                    log.Info($"[BTM_AutoSubScr]订阅默认服务时发生异常: {ex.Message}");
                    // 发生异常时不立即返回失败，继续尝试其他服务
                }

                if (result)
                {
                    log.Info("[BTM_AutoSubScr]成功订阅默认服务和特征值");
                    if (connectedDevice != null)
                    {
                        BluetoothDeviceInfo newDevice = new BluetoothDeviceInfo
                        {
                            Name = string.IsNullOrEmpty(connectedDevice.Name) ? "未知设备" : connectedDevice.Name,
                            Id = connectedDevice.DeviceId,
                            Address = connectedDevice.BluetoothAddress.ToString(),
                            IsPaired = connectedDevice.DeviceInformation.Pairing.IsPaired,
                            SignalStrength = 0
                        };
                        newDevice.IsConnected = true;

                        // 获取电量信息
                        int batteryLevel = await GetBatteryLevelAsync();
                        if (batteryLevel >= 0)
                        {
                            newDevice.BatteryLevel = batteryLevel;
                        }

                        DeviceConnected?.Invoke(this, newDevice);

                        // 返回给设备连接上的通知
                        await SendNotifyAsync();
                    }
                }
                return result;
            }
            catch (Exception ex)
            {
                log.Info($"[BTM_AutoSubScr]自动订阅服务时出错: {ex.Message}");
                ErrorOccurred?.Invoke(this, $"自动订阅服务时出错: {ex.Message}");
                return false;
            }
        }

        /// <summary>
        /// 选择服务和特征值
        /// </summary>
        /// <param name="serviceUuid">服务UUID</param>
        /// <param name="characteristicUuid">特征值UUID</param>
        /// <returns>是否成功</returns>
        public async Task<bool> SelectServiceAndCharacteristicAsync(Guid serviceUuid, Guid characteristicUuid)
        {
            if (connectedDevice == null)
            {
                log.Info("[BTM_SelectChara]未连接到设备");
                ErrorOccurred?.Invoke(this, "未连接到设备");
                return false;
            }
            int retryTimes = 5;
            try
            {
                for (int i = 1; i <= retryTimes; i++)
                {
                    // 获取指定服务
                    GattDeviceServicesResult servicesResult = await connectedDevice.GetGattServicesForUuidAsync(serviceUuid);
                    if (servicesResult.Status != GattCommunicationStatus.Success || servicesResult.Services.Count == 0)
                    {
                        log.Info($"[BTM_SelectChara]未找到服务 {serviceUuid}, 第{i}次");
                        await Task.Delay(1000);
                        continue;
                    }

                    selectedService = servicesResult.Services[0];

                    // 获取指定特征值
                    GattCharacteristicsResult characteristicsResult =
                        await selectedService.GetCharacteristicsForUuidAsync(characteristicUuid);
                    if (characteristicsResult.Status != GattCommunicationStatus.Success ||
                        characteristicsResult.Characteristics.Count == 0)
                    {
                        log.Info($"[BTM_SelectChara]未找到特征值 {characteristicUuid},第{i}次");
                        await Task.Delay(1000);
                        continue;
                    }

                    selectedCharacteristic = characteristicsResult.Characteristics[0];

                    // 如果特征值支持通知，则订阅通知
                    if (selectedCharacteristic.CharacteristicProperties.HasFlag(GattCharacteristicProperties.Notify) ||
                        selectedCharacteristic.CharacteristicProperties.HasFlag(GattCharacteristicProperties.Indicate))
                    {
                        // 订阅通知
                        GattCommunicationStatus status = await selectedCharacteristic.WriteClientCharacteristicConfigurationDescriptorAsync(
                            GattClientCharacteristicConfigurationDescriptorValue.Notify);
                        if (status != GattCommunicationStatus.Success)
                        {
                            log.Info($"[BTM_SelectChara]订阅通知失败: {status}, 第{i}次");
                            await Task.Delay(1000);
                            continue;
                        }

                        // 注册值变化事件
                        selectedCharacteristic.ValueChanged += Characteristic_ValueChanged;
                    }

                    log.Info($"[BTM_SelectChara]已选择服务 {serviceUuid} 和特征值 {characteristicUuid}");
                    return true;
                }

                log.Info($"[BTM_SelectChara]订阅指定的通知失败");
                ErrorOccurred?.Invoke(this, $"订阅指定的通知失败");
                return false;
            }
            catch (Exception ex)
            {
                log.Info($"[BTM_SelectChara]选择服务和特征值时出错: {ex.Message}");
                ErrorOccurred?.Invoke(this, $"选择服务和特征值时出错: {ex.Message}");
                return false;
            }
        }

      
        #endregion

        #region 设备监听事件

        private async void DeviceWatcher_Added(DeviceWatcher sender, DeviceInformation args)
        {
            log.Info($"[DeviceWatcher_Added]发现指定设备 {args.Name}");

            // 不判断信号强度，可能不准确，导致不去尝试连接设备了。（20260123）
            int SignalStrength = -120; // 默认信号强度
            // if (args.Properties.ContainsKey("System.Devices.Aep.SignalStrength"))
            // {
            //     var Signal = args.Properties.Single(d => d.Key == "System.Devices.Aep.SignalStrength").Value;
            //     if (Signal == null)
            //     {
            //         return;
            //     }
            //     SignalStrength = int.Parse(Signal.ToString());
            //     if (SignalStrength <= -100)
            //     {
            //         return;
            //     }
            // }
            //保存设备信息
            log.Info($"[DeviceWatcher_Added]记录设备信息 {args.Name} ID: {args.Id} Signal: {SignalStrength} dBm");
            bluetoothDevices[args.Id] = new BluetoothDeviceInfo
            {
                Name = args.Name,
                Id = args.Id,
                Address = args.Id.Split('-')[1],
                IsPaired = args.Pairing.IsPaired,
                CanPair = args.Pairing.CanPair,
                SignalStrength = SignalStrength,
                LastSeen = DateTime.Now
            };

            // 检查是否已经创建过该设备的实例
            BluetoothLEDevice bleDevice;
            if (!bleDevices.TryGetValue(args.Id, out bleDevice))
            {
                bleDevice = await BluetoothLEDevice.FromIdAsync(args.Id);
                bleDevices.TryAdd(args.Id, bleDevice);
                bleDevice.ConnectionStatusChanged += ConnectedDevice_ConnectionStatusChanged;
            }
          
            if (bleDevice.ConnectionStatus == BluetoothConnectionStatus.Connected)
            {
                connectedDevice = bleDevice;
                log.Info($"[DeviceWatcher_Added]设备状态：已连接");
                await AutoSubscribeToServiceAsync();
            }
            else
            {
                log.Info($"[DeviceWatcher_Added]设备状态：未连接");
            }
        }

        private async void DeviceWatcher_Updated(DeviceWatcher sender, DeviceInformationUpdate args)
        {
            // 更新设备信息
            log.Info($"[DeviceWatcher_Updated]设备信息已更新: {args.Id}");
            DeviceInformation deviceInformation = await DeviceInformation.CreateFromIdAsync(args.Id);
            int SignalStrength = -120;
            if (deviceInformation.Properties.ContainsKey("System.Devices.Aep.SignalStrength"))
            {
                var Signal = deviceInformation.Properties.Single(d => d.Key == "System.Devices.Aep.SignalStrength").Value;
                if (Signal != null)
                {
                    SignalStrength = int.Parse(Signal.ToString());
                }
            }
            // 更新相关值
            if (!bluetoothDevices.ContainsKey(args.Id))
            {
                bluetoothDevices[args.Id] = new BluetoothDeviceInfo
                {
                    Name = deviceInformation.Name,
                    Id = args.Id,
                    Address = args.Id.Split('-')[1],
                    IsPaired = deviceInformation.Pairing.IsPaired,
                    CanPair = deviceInformation.Pairing.CanPair,
                    SignalStrength = SignalStrength,
                    LastSeen = DateTime.Now
                };
            }
            else
            {
                BluetoothDeviceInfo bluetoothDevice = bluetoothDevices[args.Id];
                bluetoothDevice.Name = deviceInformation.Name;
                bluetoothDevice.Address = args.Id.Split('-')[1];
                bluetoothDevice.IsPaired = deviceInformation.Pairing.IsPaired;
                bluetoothDevice.CanPair = deviceInformation.Pairing.CanPair;
                bluetoothDevice.SignalStrength = SignalStrength;
                bluetoothDevice.LastSeen = DateTime.Now;
            }
            BluetoothDeviceInfo device = bluetoothDevices[args.Id];
            log.Info($"[DeviceWatcher_Updated]更新设备信息 Name: {device.Name} ID: {device.Id} Address: {device.Address} IsPaired: {device.IsPaired} CanPair: {device.CanPair} Signal: {SignalStrength} dBm");
        }

        private void DeviceWatcher_Removed(DeviceWatcher sender, DeviceInformationUpdate args)
        {
            try
            {
                log.Info($"[DeviceWatcher_Removed]设备已移除: {args.Id}");
                // 移除设备信息
                BluetoothDeviceInfo deviceInfo = null;
                if (bluetoothDevices.TryRemove(args.Id, out deviceInfo))
                {
                    // 移除事件订阅
                    BluetoothLEDevice bleDevice;
                    if (bleDevices.TryRemove(args.Id, out bleDevice))
                    {
                        try
                        {
                            bleDevice.ConnectionStatusChanged -= ConnectedDevice_ConnectionStatusChanged;
                            bleDevice.Dispose();
                        }
                        catch (Exception ex)
                        {
                            log.Info($"[DeviceWatcher_Removed]清理蓝牙设备时出错: {ex.Message}");
                        }
                    }

                    // 清理配对设备配置文件
                    if (connectedDevice != null && connectedDevice.DeviceId == deviceInfo.Id)
                    {
                        try
                        {
                            connectedDevice.Dispose();
                        }
                        catch (Exception ex)
                        {
                            log.Info($"[DeviceWatcher_Removed]清理连接设备时出错: {ex.Message}");
                        }
                        connectedDevice = null;
                    }

                    try
                    {
                        DeviceRemoved?.Invoke(this, "");
                    }
                    catch (Exception ex)
                    {
                        log.Error($"[DeviceWatcher_Removed]触发设备移除事件时出错: {ex.Message}");
                        log.Error($"[DeviceWatcher_Removed]异常类型: {ex.GetType().FullName}");
                        log.Error($"[DeviceWatcher_Removed]异常堆栈: {ex.StackTrace}");
                    }
                }
            }
            catch (Exception ex)
            {
                log.Info($"[DeviceWatcher_Removed]处理设备移除事件时出错: {ex.Message}");
                log.Error($"[DeviceWatcher_Removed]异常堆栈: {ex.StackTrace}");
            }
        }

        private async void ConnectedDevice_ConnectionStatusChanged(BluetoothLEDevice sender, object args)
        {
            try
            {
                if (sender.ConnectionStatus == BluetoothConnectionStatus.Disconnected)
                {
                    log.Info($"[ConnectionStatusChanged]蓝牙设备{sender.Name}已断开连接");
                    // 触发设备断开事件
                    BluetoothDeviceInfo newDevice = new BluetoothDeviceInfo
                    {
                        Name = string.IsNullOrEmpty(sender.DeviceInformation.Name) ? "未知设备" : sender.DeviceInformation.Name,
                        Id = sender.DeviceInformation.Id,
                        Address = sender.DeviceInformation.Id.Split('-')[1],
                        IsPaired = sender.DeviceInformation.Pairing.IsPaired,
                        CanPair = sender.DeviceInformation.Pairing.CanPair,
                        SignalStrength = -120,
                        IsConnected = false
                    };
                    DeviceDisconnected?.Invoke(this, newDevice);

                    log.Info("[ConnectionStatusChanged]设备连接状态变化为断开，主动清理资源");
                    DisconnectAsync();
                }
                else if(sender.ConnectionStatus == BluetoothConnectionStatus.Connected)
                {
                    log.Info($"[ConnectionStatusChanged]蓝牙设备{sender.Name}已连接");
                    connectedDevice = sender;
                    await AutoSubscribeToServiceAsync();
                }
            }
            catch(Exception ex)
            {
                log.Error($"[ConnectionStatusChanged]处理连接状态变化时出错: {ex.Message}");
            }
        }
       
        #endregion

        #region 数据发送和接收核心
        /// <summary>
        /// 发送数据到连接的蓝牙设备
        /// </summary>
        /// <param name="data">要发送的字符串数据</param>
        /// <returns>是否发送成功</returns>
        public async Task<bool> SendDataAsync(string data)
        {
            if (selectedCharacteristic == null)
            {
                log.Info("[BTM_SendData]未选择特征值，无法发送数据");
                ErrorOccurred?.Invoke(this, "未选择特征值，无法发送数据");
                return false;
            }

            try
            {

                using (DataWriter dataWriter = new DataWriter())
                {
                    dataWriter.WriteString(data);

                    // 发送数据
                    GattCommunicationStatus status = await selectedCharacteristic.WriteValueAsync(
                        dataWriter.DetachBuffer(),
                        GattWriteOption.WriteWithResponse);

                    if (status != GattCommunicationStatus.Success)
                    {
                        log.Info($"[BTM_SendData]发送数据失败: {status}");
                        ErrorOccurred?.Invoke(this, $"发送数据失败: {status}");
                        return false;
                    }
                }

                log.Info($"[BTM_SendData]已发送数据: {data}");
                return true;
            }
            catch (Exception ex)
            {
                log.Info($"[BTM_SendData]发送数据时出错: {ex.Message}");
                ErrorOccurred?.Invoke(this, $"发送数据时出错: {ex.Message}");
                return false;
            }
        }

        /// <summary>
        /// 发送二进制数据到连接的蓝牙设备
        /// </summary>
        /// <param name="data">要发送的二进制数据</param>
        /// <returns>是否发送成功</returns>
        public async Task<bool> SendDataAsync(byte[] data)
        {
            if (selectedCharacteristic == null)
            {
                log.Info("[BTM_SendData]未选择特征值，无法发送数据");
                ErrorOccurred?.Invoke(this, "未选择特征值，无法发送数据");
                return false;
            }

            try
            {
                using (DataWriter dataWriter = new DataWriter())
                {
                    dataWriter.WriteBytes(data);
                    // 发送数据
                    GattCommunicationStatus status = await selectedCharacteristic.WriteValueAsync(
                        dataWriter.DetachBuffer(),
                        GattWriteOption.WriteWithResponse);

                    if (status != GattCommunicationStatus.Success)
                    {
                        log.Info($"[BTM_SendData]发送数据失败: {status}");
                        ErrorOccurred?.Invoke(this, $"发送数据失败: {status}");
                        return false;
                    }
                }
                log.Info($"[BTM_SendData]已发送二进制数据，长度: {data.Length} 字节");
                return true;
            }
            catch (Exception ex)
            {
                log.Info($"[BTM_SendData]发送数据时出错: {ex.Message}");
                ErrorOccurred?.Invoke(this, $"发送数据时出错: {ex.Message}");
                return false;
            }
        }

        /// <summary>
        /// 从订阅服务收到数据
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="args"></param>
        private void Characteristic_ValueChanged(GattCharacteristic sender, GattValueChangedEventArgs args)
        {
            // 读取数据
            var reader = DataReader.FromBuffer(args.CharacteristicValue);
            byte[] data = new byte[reader.UnconsumedBufferLength];
            reader.ReadBytes(data);

            string recivedData = "";
            foreach (var b in data)
            {
                string hex = b.ToString("X2");
                recivedData += ("0x" + hex + " ");
            }
            log.Info("[BTM_CharaValue]收到蓝牙数据：" + recivedData);

            // 触发事件
            DataReceived?.Invoke(this, data);
        }
        #endregion

        #region 外部调用函数封装

        public async Task SendGetInfoCommandAsync()
        {
            if (connectedDevice == null || selectedCharacteristic == null)
            {
                log.Info("[BTM_GetInfoCmd]设备未连接或未订阅");
                return;
            }

            try
            {
                // MSG_GET_INFO命令没有附加数据，只需发送命令类型
                byte[] commandData = new byte[] { CmdMessage.MSG_GET_INFO, 0x01, 0x01 };

                await SendDataAsync(commandData);
                log.Info("[BTM_GetInfoCmd]已发送获取设备信息命令");
            }
            catch (Exception ex)
            {
                log.Error($"[BTM_GetInfoCmd]发送获取设备信息命令时出错: {ex.Message}");
                ErrorOccurred?.Invoke(this, $"发送获取设备信息命令时出错: {ex.Message}");
            }
        }

        public async Task SendGetFingerNamesAsync()
        {
            if (connectedDevice == null || selectedCharacteristic == null)
            {
                log.Info("[BTM_GetFingerNamesCmd]设备未连接或未订阅");
                return;
            }

            try
            {
                // MSG_GET_INFO命令没有附加数据，只需发送命令类型
                byte[] commandData = new byte[] { CmdMessage.MSG_GET_FINGER_NAMES, 1, 1 };

                await SendDataAsync(commandData);
                log.Info("[BTM_GetFingerNamesCmd]已发送获取所有指纹名称命令");
            }
            catch (Exception ex)
            {
                log.Error($"[BTM_GetFingerNamesCmd]发送获取所有指纹命令时出错: {ex.Message}");
                ErrorOccurred?.Invoke(this, $"发送获取所有指纹命令时出错: {ex.Message}");
            }
        }

        public async Task SendSetFingerNameAsync(byte fingerIndex, string fingerName)
        {
            if (connectedDevice == null || selectedCharacteristic == null)
            {
                log.Info("[BTM_SetFingerNameCmd]设备未连接或未订阅");
                return;
            }

            try
            {
                byte[] fingerNameBytes = Encoding.UTF8.GetBytes(fingerName);
                byte[] commandData = new byte[] { CmdMessage.MSG_SET_FINGER_NAME, fingerIndex };
                commandData = commandData.Concat(fingerNameBytes).ToArray();
                await SendDataAsync(commandData);
                log.Info("[BTM_SetFingerNameCmd]已发送设置指纹名称命令");
            }
            catch (Exception ex)
            {
                log.Error($"[BTM_SetFingerNameCmd]发送设置指纹名称命令时出错: {ex.Message}");
                ErrorOccurred?.Invoke(this, $"发送设置指纹名称命令时出错: {ex.Message}");
            }
        }

        public async Task SendSetSleepTime(int sleepTime)
        {
            if (connectedDevice == null || selectedCharacteristic == null)
            {
                log.Info("[BTM_SetSleepTimeCmd]设备未连接或未订阅");
                return;
            }

            try
            {
                byte[] commandData = new byte[5];
                commandData[0] = CmdMessage.MSG_SET_SLEEPTIME;
                commandData[1] = (byte)(sleepTime & 0xFF);
                commandData[2] = (byte)((sleepTime >> 8) & 0xFF);
                commandData[3] = (byte)((sleepTime >> 16) & 0xFF);
                commandData[4] = (byte)((sleepTime >> 24) & 0xFF);

                await SendDataAsync(commandData);
                log.Info("[BTM_SetSleepTimeCmd]已发送设置休眠时间命令");
            }
            catch (Exception ex)
            {
                log.Error($"[BTM_SetSleepTimeCmd]发送设置休眠时间命令时出错: {ex.Message}");
                ErrorOccurred?.Invoke(this, $"发送设置休眠时间命令时出错: {ex.Message}");
            }
        }

        /// <summary>
        /// 发送锁屏状态
        /// </summary>
        public async Task SendScreenLockAsync(byte isScreenLocked)
        {
            if (connectedDevice == null || selectedCharacteristic == null)
            {
                log.Info("[BTM_ScreenLockCmd]设备未连接或未订阅");
                return;
            }

            try
            {
                byte[] commandData = new byte[] { CmdMessage.MSG_LOCKSCREEN_STATUS, isScreenLocked };
                await SendDataAsync(commandData);
                log.Info("[BTM_ScreenLockCmd]已发送是否锁屏查询命令的响应");
            }
            catch (Exception ex)
            {
                log.Error($"[BTM_ScreenLockCmd]发送锁屏状态命令时出错: {ex.Message}");
                ErrorOccurred?.Invoke(this, $"发送锁屏状态命令时出错: {ex.Message}");
            }
        }

        public async Task SendCheckSleepResponseAsync(byte canSleep)
        {
            if (connectedDevice == null || selectedCharacteristic == null)
            {
                log.Info("[BTM_CheckSleepCmd]设备未连接或未订阅");
                return;
            }

            try
            {
                byte[] commandData = new byte[2];
                commandData[0] = CmdMessage.MSG_CHECK_SLEEP;
                commandData[1] = canSleep;
                await SendDataAsync(commandData);
                log.Info($"[BTM_CheckSleepCmd]已发送检查休眠响应: {canSleep}");
            }
            catch (Exception ex)
            {
                log.Error($"[BTM_CheckSleepCmd]发送检查休眠响应时出错: {ex.Message}");
                ErrorOccurred?.Invoke(this, $"发送检查休眠响应时出错: {ex.Message}");
            }
        }

        /// <summary>
        /// 发送订阅成功通知
        /// </summary>
        /// <returns></returns>
        public async Task SendNotifyAsync()
        {
            if (connectedDevice == null || selectedCharacteristic == null)
            {
                log.Info("[BTM_NotifyCmd]设备未连接或未订阅");
                return;
            }

            try
            {
                byte[] commandData = new byte[] { CmdMessage.MSG_DEVICE_NOTIFY, 0x01 };
                await SendDataAsync(commandData);
                log.Info("[BTM_NotifyCmd]已发送订阅成功通知");
            }
            catch (Exception ex)
            {
                log.Error($"[BTM_NotifyCmd]发送通知命令时出错: {ex.Message}");
                ErrorOccurred?.Invoke(this, $"发送通知命令时出错: {ex.Message}");
            }
        }

        /// <summary>
        /// 发送允许休眠状态命令
        /// </summary>
        /// <param name="enableSleep"></param>
        /// <returns></returns>
        public async Task SendEnableSleepAsync(byte enableSleep)
        {
            if (connectedDevice == null || selectedCharacteristic == null)
            {
                log.Info("[BTM_EnableSleepCmd]设备未连接或未订阅");
                return;
            }

            try
            {
                byte[] commandData = new byte[] { CmdMessage.MSG_ENALBE_SLEEP, enableSleep };
                await SendDataAsync(commandData);
                log.Info("[BTM_EnableSleepCmd]已发送允许休眠状态命令");
            }
            catch (Exception ex)
            {
                log.Error($"[BTM_EnableSleepCmd]发送允许休眠状态命令时出错: {ex.Message}");
                ErrorOccurred?.Invoke(this, $"发送允许休眠状态命令时出错: {ex.Message}");
            }
        }

        /// <summary>
        /// 发送固件升级开始命令
        /// </summary>
        /// <param name="fileLength"></param>
        /// <returns></returns>
        public async Task SendFirmwareUpdateStartAsync(byte[] fileLength)
        {
            if (connectedDevice == null || selectedCharacteristic == null)
            {
                log.Info("[BTM_SendFirmwareUpdateStart]设备未连接或未订阅");
                return;
            }

            try
            {
                byte[] commandData = new byte[] { CmdMessage.MSG_FIRMWARE_UPDATE_START };
                commandData = commandData.Concat(fileLength).ToArray();
                log.Info("[BTM_SendFirmwareUpdateStart]发送固件升级开始命令");
                await SendDataAsync(commandData);
                log.Info("[BTM_SendFirmwareUpdateStart]完成发送固件升级开始命令");
            }
            catch (Exception ex)
            {
                log.Error($"[BTM_SendFirmwareUpdateStart]发送固件升级开始命令出错: {ex.Message}");
                ErrorOccurred?.Invoke(this, $"发送固件升级开始命令出错: {ex.Message}");
            }
        }

        public async Task SendFirmwareUpdateChunkAsync(byte[] firmwareData)
        {
            if (connectedDevice == null || selectedCharacteristic == null)
            {
                log.Info("[BTM_SendFirmwareUpdateChunk]设备未连接或未订阅");
                return;
            }

            try
            {
                byte[] commandData = new byte[] { CmdMessage.MSG_FIRMWARE_UPDATE_CHUNK };
                commandData = commandData.Concat(firmwareData).ToArray();
                //log.Info("[BTM_SendFirmwareUpdateChunk]开始发送固件数据");
                await SendDataAsync(commandData);
                //log.Info("[BTM_SendFirmwareUpdateChunk]已完成发送固件数据");
            }
            catch (Exception ex)
            {
                log.Error($"[BTM_SendFirmwareUpdateChunk]发送固件数据时出错: {ex.Message}");
                ErrorOccurred?.Invoke(this, $"发送固件数据时出错: {ex.Message}");
            }
        }

        public async Task SendFirmwareUpdateEndAsync(byte[] crc32Value)
        {
            if (connectedDevice == null || selectedCharacteristic == null)
            {
                log.Info("[BTM_SendFirmwareUpdateEnd]设备未连接或未订阅");
                return;
            }

            try
            {
                byte[] commandData = new byte[] { CmdMessage.MSG_FIRMWARE_UPDATE_END };
                commandData = commandData.Concat(crc32Value).ToArray();
              
                log.Info("[BTM_SendFirmwareUpdateEnd]开始发送固件更新完成命令");
                await SendDataAsync(commandData);
                log.Info("[BTM_SendFirmwareUpdateEnd]完成发送固件更新完成命令");
            }
            catch (Exception ex)
            {
                log.Error($"[BTM_SendFirmwareUpdateEnd]发送固件更新完成命令出错: {ex.Message}");
                ErrorOccurred?.Invoke(this, $"发送固件更新完成命令出错: {ex.Message}");
            }
        }

        public string GetDeviceId()
        {
            // 遍历bluetoothDevices
            string deviceId = "";
            DateTime lastSeen = DateTime.MinValue;

            foreach (var device in bluetoothDevices.Values)
            {
                if (!device.IsPaired && device.CanPair)
                {
                    // 优先选择最近发现/更新的设备
                    if (device.LastSeen > lastSeen)
                    {
                        lastSeen = device.LastSeen;
                        deviceId = device.Id;
                    }
                }
            }
            return deviceId;
        }
        /// <summary>
        /// 获取已连接设备的电量信息
        /// </summary>
        /// <returns>电量百分比，如果获取失败则返回-1</returns>
        public async Task<int> GetBatteryLevelAsync()
        {
            try
            {
                if (connectedDevice == null)
                {
                    log.Info("[BTM_GetBattery]未连接设备，无法获取电量");
                    return -1;
                }

                // 获取标准电量服务
                GattDeviceServicesResult servicesResult = await connectedDevice.GetGattServicesForUuidAsync(BATTERY_SERVICE_UUID);

                if (servicesResult.Status != GattCommunicationStatus.Success || servicesResult.Services.Count == 0)
                {
                    log.Info("[BTM_GetBattery]未找到电量服务");
                    return -1;
                }

                // 获取电量特征值
                var batteryService = servicesResult.Services[0];
                GattCharacteristicsResult characteristicsResult = await batteryService.GetCharacteristicsForUuidAsync(BATTERY_CHARACTERISTIC_UUID);

                if (characteristicsResult.Status != GattCommunicationStatus.Success || characteristicsResult.Characteristics.Count == 0)
                {
                    log.Info("[BTM_GetBattery]未找到电量特征值");
                    batteryService.Dispose();
                    return -1;
                }

                // 读取电量值
                var batteryCharacteristic = characteristicsResult.Characteristics[0];
                GattReadResult readResult = await batteryCharacteristic.ReadValueAsync(BluetoothCacheMode.Uncached);

                if (readResult.Status != GattCommunicationStatus.Success)
                {
                    log.Info($"[BTM_GetBattery]读取电量失败: {readResult.Status}");
                    batteryService.Dispose();
                    return -1;
                }

                // 解析电量值（电量值是一个字节，表示百分比）
                var reader = DataReader.FromBuffer(readResult.Value);
                byte batteryLevel = reader.ReadByte();

                log.Info($"[BTM_GetBattery]设备: {connectedDevice.Name} 电量: {batteryLevel}%");
                batteryService.Dispose();

                return batteryLevel;
            }
            catch (Exception ex)
            {
                log.Error($"[BTM_GetBattery]获取电量信息时出错: {ex.Message}");
                return -1;
            }
        }

        public string GetPairedDevice()
        {
            string deviceName = "";
            bool isPaired = false;
            string deviceId  = "";
            
            try
            {
                foreach (var device in bluetoothDevices.Values)
                {
                    if (device.IsPaired)
                    {
                        deviceName = device.Name;
                        isPaired = true;
                        deviceId = device.Id;
                        break;
                    }
                }
            }
            catch (Exception ex)
            {
                log.Info($"[GetPairedDevice]获取配对设备时出错: {ex.Message}");
            }

            return deviceName + "|" + isPaired.ToString() + "|" + deviceId;
        }
        #endregion
    }
}
