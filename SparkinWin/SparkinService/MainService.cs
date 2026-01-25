using Microsoft.Win32;
using NLog;
using SparkinLib;
using SparkinLib.Bluetooth;
using SparkinLib.Tools;
using System;
using System.Linq;
using System.ServiceProcess;
using System.Threading.Tasks;
/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
namespace SparkinService
{
    public partial class MainService : ServiceBase, IPipeCallback
    {
        // 蓝牙管理器
        private BluetoothManager bluetoothManager;
        private BluetoothDeviceInfo bluetoothDeviceInfo = null;
        // 管道服务
        private PipeServer pipeServer;
        // 配置文件
        private ConfigFile configFile = null;
        // 日志记录器
        private Logger log = LogUtil.GetLogger();

        public MainService()
        {
            InitializeComponent();
        }

        protected override void OnStart(string[] args)
        {
            log.Info("[Service_OnStart]Sparkin Service 启动中...");

            var initializationTask = Task.Run(() =>
            {
                try
                {
                    log.Info("[Service_OnStart]步骤1: 初始化管道服务器");
                    pipeServer = new PipeServer(this);
                    pipeServer.Start();

                    log.Info("[Service_OnStart]步骤2: 初始化蓝牙管理器");
                    InitializeBluetoothManager();

                    log.Info("[Service_OnStart]步骤3: 开始监听蓝牙设备");
                    bluetoothManager.StartMonitoringPairedDevices();

                    log.Info("[Service_OnStart]步骤4: 加载配置文件");
                    configFile = ConfigManager.LoadConfigFile(log);

                    log.Info("[Service_OnStart]Sparkin Service 启动成功");
                }
                catch (Exception ex)
                {
                    log.Error($"[Service_OnStart]服务启动失败: {ex.Message}");
                    log.Error($"[Service_OnStart]异常类型: {ex.GetType().FullName}");
                    log.Error($"[Service_OnStart]异常堆栈: {ex.StackTrace}");
                    
                    if (ex.InnerException != null)
                    {
                        log.Error($"[Service_OnStart]内部异常: {ex.InnerException.Message}");
                        log.Error($"[Service_OnStart]内部异常堆栈: {ex.InnerException.StackTrace}");
                    }

                    log.Error("[Service_OnStart]服务启动失败，正在停止服务...");
                    
                    try
                    {
                        this.Stop();
                    }
                    catch (Exception stopEx)
                    {
                        log.Error($"[Service_OnStart]停止服务时出错: {stopEx.Message}");
                    }
                }
            });

            var timeoutTask = Task.Delay(30000);
            var completedTask = Task.WhenAny(initializationTask, timeoutTask).Result;

            if (completedTask == timeoutTask)
            {
                log.Error("[Service_OnStart]服务启动超时（30秒），停止服务");
                try
                {
                    this.Stop();
                }
                catch (Exception stopEx)
                {
                    log.Error($"[Service_OnStart]停止服务时出错: {stopEx.Message}");
                }
            }
        }

        protected override void OnStop()
        {
            try
            {
                log.Info("[Service_OnStop]Sparkin Service 停止");
              
                // 停止蓝牙监听
                if (bluetoothManager != null)
                {
                    bluetoothManager.StopMonitoringPairedDevices();
                }
                
                // 停止管道服务
                if (pipeServer != null)
                {
                    pipeServer.Stop();
                }

                LogManager.Flush();
                LogManager.Shutdown();
            }
            catch (Exception ex)
            {
                log.Error($"[Service_OnStop]服务停止时出错: {ex.Message}");
            }
        }
        
        #region 蓝牙相关
        private void InitializeBluetoothManager()
        {
            bluetoothManager = new BluetoothManager();

            // 注册事件处理程序
            bluetoothManager.DeviceConnected += BluetoothManager_DeviceConnected;
            bluetoothManager.DeviceDisconnected += BluetoothManager_DeviceDisconnected;
            bluetoothManager.DataReceived += BluetoothManager_DataReceived;
            bluetoothManager.ErrorOccurred += BluetoothManager_ErrorOccurred;
            bluetoothManager.DeviceRemoved += BluetoothManager_DeviceRemoved;
        }

        private void BluetoothManager_DeviceRemoved(object sender, string e)
        {
            // 通知客户端,重新获取配对状态
            if (pipeServer != null)
            {
                try
                {
                    log.Info($"[PIPE]准备发送设备移除通知，获取配对状态");
                    string pairedDevice = bluetoothManager.GetPairedDevice();
                    log.Info($"[PIPE]设备状态：" + pairedDevice);
                    PipeMessage pipeMsg = new PipeMessage
                    {
                        Type = PipeMessage.MessageType.BluetoothPairStatus,
                        StringData = pairedDevice
                    };
                    pipeServer.SendMessage(pipeMsg);
                }
                catch (Exception ex)
                {
                    log.Error($"[BluetoothManager_DeviceRemoved]处理设备移除通知时出错: {ex.Message}");
                    log.Error($"[BluetoothManager_DeviceRemoved]异常类型: {ex.GetType().FullName}");
                    log.Error($"[BluetoothManager_DeviceRemoved]异常堆栈: {ex.StackTrace}");
                    
                    // 即使出错，也要发送空状态通知客户端
                    try
                    {
                        PipeMessage pipeMsg = new PipeMessage
                        {
                            Type = PipeMessage.MessageType.BluetoothPairStatus,
                            StringData = "|False|"
                        };
                        pipeServer.SendMessage(pipeMsg);
                    }
                    catch (Exception sendEx)
                    {
                        log.Error($"[BluetoothManager_DeviceRemoved]发送设备移除通知失败: {sendEx.Message}");
                    }
                }
            }
        }

        private void BluetoothManager_DeviceConnected(object sender, BluetoothDeviceInfo deviceInfo)
        {
            log.Info($"[BT_DeviceConnected]连接到设备: {deviceInfo.Name}，电量: {deviceInfo.BatteryLevel}%");
            bluetoothDeviceInfo = deviceInfo;
            // 通知客户端
            if (pipeServer != null)
            {
                PipeMessage message = new PipeMessage
                {
                    Type = PipeMessage.MessageType.BluetoothDeviceConnected,
                    StringData = deviceInfo.Name + "|" + deviceInfo.BatteryLevel 
                };
                pipeServer.SendMessage(message);
            }
        }
        
        private void BluetoothManager_DeviceDisconnected(object sender, BluetoothDeviceInfo deviceInfo)
        {
            log.Info($"[BT_DeviceDisconnected]设备已断开连接: {deviceInfo.Name}");
            bluetoothDeviceInfo = null;
            // 通知客户端
            if (pipeServer != null)
            {
                PipeMessage message = new PipeMessage
                {
                    Type = PipeMessage.MessageType.BluetoothDeviceDisconnected,
                    StringData = deviceInfo.Name
                };
                pipeServer.SendMessage(message);
            }
        }
        
        private void BluetoothManager_DataReceived(object sender, byte[] data)
        {
            log.Info("[BT_DataReceived]收到蓝牙数据");

            if (data.Length >= 4)
            {
                DataHeader header = DataHeader.GetDataHeader(data);
                switch(header.cmd)
                {
                    case CmdMessage.MSG_FINGERPRINT_SEARCH:
                        log.Info("[BT_DataReceived]收到解锁屏幕请求");

                        // 解锁屏幕
                        if (!string.IsNullOrEmpty(configFile.LoginUserName) && !string.IsNullOrEmpty(configFile.LoginPassword))
                        {
                            ScreenUnlocker.Unlock(configFile.LoginUserName, configFile.LoginPassword, log);
                        }
                        break;
                        
                    case CmdMessage.MSG_LOCKSCREEN_STATUS:
                        log.Info("[BT_DataReceived]收到锁屏状态请求");
                        
                        // 发送锁屏状态
                        Task.Run(async () => {
                            byte lockState = (byte)(Utils.IsSystemLocked() ? 1 : 0);
                            log.Info($"[BT_DataReceived]当前系统锁屏状态：{lockState}");
                            await bluetoothManager.SendScreenLockAsync(lockState);
                        });
                        break;
                    case CmdMessage.MSG_FINGERPRINT_REGISTER:
                    case CmdMessage.MSG_FINGERPRINT_DELETE:
                    case CmdMessage.MSG_PUT_FINGER:
                    case CmdMessage.MSG_REMOVE_FINGER:
                    case CmdMessage.MSG_GET_INFO:
                    case CmdMessage.MSG_FINGERPRINT_REGISTER_CANCEL:
                    case CmdMessage.MSG_SET_SLEEPTIME:
                    case CmdMessage.MSG_SET_FINGER_NAME:
                    case CmdMessage.MSG_GET_FINGER_NAMES:
                    case CmdMessage.MSG_RENAME_FINGER_NAME:
                    case CmdMessage.MSG_ENALBE_SLEEP:
                    case CmdMessage.MSG_FIRMWARE_UPDATE_START:
                    case CmdMessage.MSG_FIRMWARE_UPDATE_CHUNK:
                    case CmdMessage.MSG_FIRMWARE_UPDATE_END:
                        // 将这些数据转发给客户端
                        if (pipeServer != null)
                        {
                            log.Info("[BT_DataReceived]蓝牙数据通过通道转发给客户端");
                            PipeMessage message = new PipeMessage
                            {
                                Type = PipeMessage.MessageType.BluetoothDataReceived,
                                Data = data
                            };
                            pipeServer.SendMessage(message);
                        }
                        break;
                        
                    case CmdMessage.MSG_CHECK_SLEEP:
                        log.Info("[BT_DataReceived]收到检查休眠请求");
                        
                        // 检查管道是否存在（客户端是否连接）
                        if (pipeServer != null && pipeServer.IsConnected())
                        {
                            log.Info("[BT_DataReceived]管道已连接，向客户端发送检查休眠请求");
                            PipeMessage checkSleepRequest = new PipeMessage
                            {
                                Type = PipeMessage.MessageType.CheckSleepRequest
                            };
                            pipeServer.SendMessage(checkSleepRequest);
                        }
                        else
                        {
                            log.Info("[BT_DataReceived]管道未连接，UI未运行，可以休眠");
                            Task.Run(async () => {
                                await bluetoothManager.SendCheckSleepResponseAsync(1);
                            });
                        }
                        break;
                    default:
                        log.Info("[BT_DataReceived]未知的蓝牙数据HEADER：0x" + header.cmd.ToString("X2"));
                        break;
                }
            }
        }
        
        private void BluetoothManager_ErrorOccurred(object sender, string errorMessage)
        {
            log.Error($"[BT_ErrorOccurred]蓝牙错误: {errorMessage}");
            
            // 通知客户端
            if (pipeServer != null)
            {
                PipeMessage message = new PipeMessage
                {
                    Type = PipeMessage.MessageType.BluetoothError,
                    StringData = errorMessage
                };
                pipeServer.SendMessage(message);
            }
        }
        #endregion
        
        #region 管道通信
        public void OnMessageReceived(PipeMessage message)
        {
            try
            {
                log.Info($"[PIPE]收到管道消息: {message.Type}");
                
                switch (message.Type)
                {
                    case PipeMessage.MessageType.ConnectBluetooth:
                        {
                            log.Info($"[PIPE]开始执行配对操作");
                            string deviceId = bluetoothManager.GetDeviceId();
                            log.Info($"[PIPE]返回设备ID给客户端：{deviceId}");
                            PipeMessage pipeMsg = new PipeMessage
                            {
                                Type = PipeMessage.MessageType.BluetoothPair,
                                StringData = deviceId
                            };
                            pipeServer.SendMessage(pipeMsg);
                            break;
                        }
                    case PipeMessage.MessageType.DisconnectBluetooth:
                        log.Info($"[PIPE]开始执行断开设备操作");
                        bluetoothManager.DisconnectAsync();
                        break;
                        
                    case PipeMessage.MessageType.GetDeviceInfo:
                        log.Info($"[PIPE]开始获取蓝牙设备信息操作");
                        Task.Run(async () => {
                            await bluetoothManager.SendGetInfoCommandAsync();
                        });
                        break;
                    case PipeMessage.MessageType.GetConnectState:
                        log.Info($"[PIPE]开始获取蓝牙设备连接状态操作");
                        Task.Run(async () => {
                            if (bluetoothDeviceInfo != null)
                            {
                                log.Info($"[PIPE]设备已连接，开始返回状态数据");
                                int batteryLevel = await bluetoothManager.GetBatteryLevelAsync();
                                if (batteryLevel > -1)
                                {
                                    PipeMessage pipeMsg = new PipeMessage
                                    {
                                        Type = PipeMessage.MessageType.BluetoothDeviceConnected,
                                        StringData = bluetoothDeviceInfo.Name + "|" + batteryLevel
                                    };
                                    pipeServer.SendMessage(pipeMsg);
                                }
                            }
                            else
                            {
                                log.Info($"[PIPE]设备未连接，开始返回状态数据");
                                PipeMessage pipeMsg = new PipeMessage
                                {
                                    Type = PipeMessage.MessageType.BluetoothDeviceDisconnected,
                                    StringData = ""
                                };
                                pipeServer.SendMessage(pipeMsg);
                            }
                        });
                        break;
                    case PipeMessage.MessageType.SendBluetoothCommand:
                        log.Info($"[PIPE]转发客户端数据到蓝牙设备");
                        log.Info($"[PIPE]转发的值：[{string.Join(", ", message.Data.Select(b => "0x" + b.ToString("X2")))}]");
                        if (message.Data != null)
                        {
                            Task.Run(async () => {
                                await bluetoothManager.SendDataAsync(message.Data);
                            });
                        }
                        break;
                    case PipeMessage.MessageType.BluetoothPairStatus:
                        {
                            log.Info($"[PIPE]开始获取设备配对状态");
                            string pairedDevice = bluetoothManager.GetPairedDevice();
                            log.Info($"[PIPE]设备状态：" + pairedDevice);
                            PipeMessage pipeMsg = new PipeMessage
                            {
                                Type = PipeMessage.MessageType.BluetoothPairStatus,
                                StringData = pairedDevice
                            };
                            pipeServer.SendMessage(pipeMsg);
                        }
                        break;
                    case PipeMessage.MessageType.BluetoothUnPair:
                        {
                            log.Info($"[PIPE]开始执行取消配对操作");
                            string pairedDevice = bluetoothManager.GetPairedDevice();
                            log.Info($"[PIPE]设备状态：" + pairedDevice);
                            string[] data = pairedDevice.Split('|');
                            string deviceId = "";
                            if(data.Length == 3)
                                deviceId = data[2];
                            PipeMessage pipeMsg = new PipeMessage
                            {
                                Type = PipeMessage.MessageType.BluetoothUnPair,
                                StringData = deviceId
                            };
                            pipeServer.SendMessage(pipeMsg);
                        }
                        break;
                    case PipeMessage.MessageType.SetSleepTime:
                        log.Info($"[PIPE]开始执行设置休眠时间的操作");
                        if (!string.IsNullOrEmpty(message.StringData) && int.TryParse(message.StringData, out int sleepTime))
                        {
                            log.Info($"[PIPE]转发给蓝牙设备,设休眠时间为{sleepTime}秒");
                            Task.Run(async () => {
                                await bluetoothManager.SendSetSleepTime(sleepTime);
                            });
                        }
                        break;
                        
                    case PipeMessage.MessageType.SetFingerName:
                        log.Info($"[PIPE]开始执行设置指纹名称的操作");
                        if (message.Data != null && message.Data.Length > 0 && !string.IsNullOrEmpty(message.StringData))
                        {
                            log.Info($"[PIPE]给蓝牙设备发送指令，设ID：{message.Data[0]} 指纹名字为：{message.StringData}");
                            Task.Run(async () => {
                                await bluetoothManager.SendSetFingerNameAsync(message.Data[0], message.StringData);
                            });
                        }
                        break;
                        
                    case PipeMessage.MessageType.GetFingerNames:
                        log.Info($"[PIPE]开始执行获取所有指纹名称的操作");
                        Task.Run(async () => {
                            await bluetoothManager.SendGetFingerNamesAsync();
                        });
                        break;
                        
                    case PipeMessage.MessageType.GetLockScreenStatus:
                        log.Info($"[PIPE]开始执行获取当前锁屏状态的操作");
                        // 返回锁屏状态
                        if (pipeServer != null)
                        {
                            PipeMessage responseMessage = new PipeMessage
                            {
                                Type = PipeMessage.MessageType.BluetoothDataReceived,
                                Data = new byte[] { CmdMessage.MSG_LOCKSCREEN_STATUS, 0x01, 0x00, (byte)(Utils.IsSystemLocked() ? 1 : 0) }
                            };
                            pipeServer.SendMessage(responseMessage);
                        }
                        break;
                    case PipeMessage.MessageType.SetEnableSleep:
                        log.Info($"[PIPE]开始执行设置是否允许设备休眠的操作");
                        if (message.Data.Length == 1)
                        {
                            log.Info($"[PIPE]转发给蓝牙设备,是否允许休眠：{message.Data[0]}");
                            Task.Run(async () =>
                            {
                                await bluetoothManager.SendEnableSleepAsync(message.Data[0]);
                            });
                        }
                        break;
                    case PipeMessage.MessageType.ReloadConfig:
                        log.Info($"[PIPE]开始重新加载配置文件");
                        try
                        {
                            configFile = ConfigManager.LoadConfigFile(log);
                            log.Info($"[PIPE]配置文件加载成功");
                        }
                        catch (Exception ex)
                        {
                            log.Error($"[PIPE]重新加载配置文件时出错: {ex.Message}");
                        }
                        break;
                    case PipeMessage.MessageType.FirmwareUpdateStart:
                        {
                            //log.Info($"[PIPE]发送固件更新开始命令");
                            if (message.Data.Length > 1)
                            {
                                Task.Run(async () =>
                                {
                                    await bluetoothManager.SendFirmwareUpdateStartAsync(message.Data);
                                });
                            }
                            break;
                        }
                    case PipeMessage.MessageType.FirmwareUpdateChunk:
                        {
                            //log.Info($"[PIPE]开始传输固件数据");
                            if (message.Data.Length > 1)
                            {
                                Task.Run(async () =>
                                {
                                    await bluetoothManager.SendFirmwareUpdateChunkAsync(message.Data);
                                });
                            }
                            break;
                        }
                    case PipeMessage.MessageType.FirmwareUpdateEnd:
                        {
                           // log.Info($"[PIPE]开始传输固件数据");
                            if (message.Data.Length > 1)
                            {
                                Task.Run(async () =>
                                {
                                    await bluetoothManager.SendFirmwareUpdateEndAsync(message.Data);
                                });
                            }
                            break;
                        }
                    case PipeMessage.MessageType.CheckSleepResponse:
                        log.Info($"[PIPE]收到客户端检查休眠响应");
                        if (message.Data != null && message.Data.Length > 0)
                        {
                            byte canSleep = message.Data[0];
                            log.Info($"[PIPE]客户端返回检查休眠值: {canSleep}");
                            Task.Run(async () =>
                            {
                                try
                                {
                                    await bluetoothManager.SendCheckSleepResponseAsync(canSleep);
                                    log.Info($"[PIPE]已转发检查休眠响应给蓝牙设备: {canSleep}");
                                }
                                catch (Exception ex)
                                {
                                    log.Error($"[PIPE]转发检查休眠响应失败: {ex.Message}");
                                }
                            });
                        }
                        else
                        {
                            log.Error("[PIPE]CheckSleepResponse 数据为空");
                        }
                        break;
                }
            }
            catch (Exception ex)
            {
                log.Error($"处理管道消息时出错: {ex.Message}");
            }
        }
        #endregion
    }
}
