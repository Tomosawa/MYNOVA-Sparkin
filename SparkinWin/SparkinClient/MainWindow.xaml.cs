using NLog;
using SparkinClient.ViewModel;
using SparkinLib;
using SparkinLib.Bluetooth;
using SparkinLib.Structs;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.IO.Pipes;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Forms;
using System.Windows.Input;
using System.Windows.Markup;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using Application = System.Windows.Application;
using ContextMenu = System.Windows.Controls.ContextMenu;
using Cursors = System.Windows.Input.Cursors;
using Grid = System.Windows.Controls.Grid;
using Image = System.Windows.Controls.Image;
using MenuItem = System.Windows.Controls.MenuItem;
using MessageBox = System.Windows.MessageBox;
using Orientation = System.Windows.Controls.Orientation;
using StackPanel = System.Windows.Controls.StackPanel;
using TextBlock = System.Windows.Controls.TextBlock;
/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
namespace SparkinClient
{
    public partial class MainWindow : IPipeCallback
    {
        // 管道客户端
        private PipeClient pipeClient;
        private bool isConnectedToService = false;
        
        // 睡眠时间相关
        private ObservableCollection<SleepTimeItem> sleepTimeItem { get; set; }
        private SleepTimeModel sleepTimeModel;
        public int SleepTimeValue;
        
        // 指纹管理相关变量
        private Dictionary<int, string> fingerNames = new Dictionary<int, string>();
        
        // 可存储最大指纹数量
        private int MAX_FINGER_NUM = 10;
        private int fingerprintCount = 0;
        
        private ConfigFile configFile = null;
        
        // 配对按钮文字
        private readonly string BTN_TEXT_PAIR = "配对设备";
        private readonly string BTN_TEXT_UNPAIR = "解除配对";

        // 配对设备ID
        private string pairedDeviceId = "";

        // 系统托盘相关变量
        private System.Windows.Forms.NotifyIcon notifyIcon;
        private bool isClosing = false;
        private ContextMenu trayIconContextMenu;

        private BluetoothManager bluetoothManager = new BluetoothManager();

        private UpdateChecker firmwareUpdater = new UpdateChecker(UpdateChecker.UpdateType.Firmware);
        private UpdateChecker clientUpdater = new UpdateChecker(UpdateChecker.UpdateType.Software);
        // 等待执行事件
        private AutoResetEvent waitEvent = new AutoResetEvent(false);

        // 日志记录
        private Logger log = LogUtil.GetLogger();

        public MainWindow()
        {
            InitializeComponent();
           
            // 初始化配置文件
            configFile = ConfigManager.LoadConfigFile(log);
            if (string.IsNullOrEmpty(configFile.LoginUserName))
            {
                configFile.LoginUserName = Environment.UserName;
            }
            txtUsername.Text = configFile.LoginUserName;
            txtPassword.Text = configFile.LoginPassword;

            btnConnect.Content = BTN_TEXT_PAIR;
            // 初始化睡眠时间数据
            sleepTimeModel = new SleepTimeModel(); 
          
            cbSleepTime.ItemsSource = sleepTimeModel.Items;
            cbSleepTime.SelectedValuePath = "Value";
            cbSleepTime.SelectedValue = 0;
            cbSleepTime.SelectionChanged += SleepTime_SelectionChanged;
            
            // 初始化管道客户端
            InitializePipeClient();
            
            // 初始化系统托盘图标
            InitializeTrayIcon();

            // 处理窗口关闭事件
            this.Closing += MainWindow_Closing;

            // 更新相关初始化
            firmwareUpdater.UpdateAvailable += FirmwareUpdater_UpdateAvailable;
            firmwareUpdater.DownloadProgress += FirmwareUpdater_DownloadProgress;
            firmwareUpdater.DownloadCompleted += FirmwareUpdater_DownloadCompleted;
            clientUpdater.UpdateAvailable += ClientUpdater_UpdateAvailable;


            // 标题显示版本号
            Version version = Assembly.GetExecutingAssembly().GetName().Version;
            this.Title += $" V{version.Major}.{version.Minor}";

            log.Info("程序已启动");
        }



        /// <summary>
        /// 窗口初始化完成
        /// </summary>
        /// <param name = "sender" ></ param >
        /// < param name="e"></param>
        private async void MainWindow_ContentRendered(object sender, EventArgs e)
        {
            Version version = Assembly.GetExecutingAssembly().GetName().Version;
            await clientUpdater.CheckForUpdateAsync($"{version.Major}.{version.Minor}");
        }

        #region 程序更新
        private void ClientUpdater_UpdateAvailable(object sender, UpdateInfo e)
        {
            log.Info($"发现新程序版本: {e.Version}");
            updateInfo = e;
            Dispatcher.Invoke(() =>
            {
                clientUpdater.UpdateUnavailable -= ClientUpdater_UpdateUnavailable;
                string strInfo = $"发现最新版本程序：V{updateInfo.Version}\r\n更新内容：\r\n{updateInfo.Description}\r\n\r\n要立即下载并开始更新吗？";
                if (MessageBox.Show(strInfo, "程序更新", MessageBoxButton.YesNo, MessageBoxImage.Question) == MessageBoxResult.Yes)
                {
                  UpdateWindow updateWindow = new UpdateWindow(updateInfo);
                    updateWindow.Owner = this;
                    if (updateWindow.ShowDialog() == true)
                    {
                        if(MessageBox.Show("更新包下载完成，立即安装更新吗？","安装更新",MessageBoxButton.YesNo, MessageBoxImage.Question) == MessageBoxResult.Yes)
                        {
                            try
                            {
                                // 创建进程启动信息
                                ProcessStartInfo startInfo = new ProcessStartInfo();
                                startInfo.FileName = updateWindow.downloadPath;  // 指定EXE文件路径
                                // 启动进程
                                Process process = Process.Start(startInfo);

                                if (process != null)
                                {
                                    log.Info("程序已启动，进程ID: " + process.Id);
                                    isClosing = true;
                                    Close();
                                }
                            }
                            catch (Exception ex)
                            {
                                log.Error("启动程序时发生错误: " + ex.Message);
                            }
                        }
                    }
                }
            });
        }

        private void ClientUpdater_UpdateUnavailable(object sender, UpdateInfo e)
        {
            Dispatcher.Invoke(() =>
            {
                clientUpdater.UpdateUnavailable -= ClientUpdater_UpdateUnavailable;
                MessageBox.Show("当前已经是最新版本，无需更新！", "程序更新", MessageBoxButton.OK, MessageBoxImage.Information);
            });
        }
        #endregion

        #region 固件更新相关
        private UpdateInfo updateInfo = null;
        private void FirmwareUpdater_DownloadCompleted(object sender, string e)
        {
            log.Info($"[DOWNLOAD_COMPLETED]下载完成，文件路径: {e}");
            string crc32Value = CRC32Tool.CalculateFileCrc32(e);
            if (!string.IsNullOrEmpty(crc32Value) && crc32Value.Equals(updateInfo.HashCRC32,StringComparison.OrdinalIgnoreCase))
            {
                log.Info($"[DOWNLOAD_COMPLETED]CRC32校验成功，开始传输固件更新");

                Task.Run(() =>
                {
                    try
                    {
                        using (FileStream fs = new FileStream(e, FileMode.Open, FileAccess.Read))
                        {
                            // 压缩固件数据
                            byte[] compressedData;
                            using (var compressedStream = new MemoryStream())
                            {
                                using (var deflate = new DeflateStream(compressedStream, CompressionMode.Compress))
                                {
                                    fs.CopyTo(deflate);
                                }
                                compressedData = compressedStream.ToArray();
                            }

                            int fileLength = compressedData.Length;
                            byte[] dataLength = new byte[4];
                            dataLength[0] = (byte)(fileLength & 0xFF);
                            dataLength[1] = (byte)((fileLength >> 8) & 0xFF);
                            dataLength[2] = (byte)((fileLength >> 16) & 0xFF);
                            dataLength[3] = (byte)((fileLength >> 24) & 0xFF);

                            pipeClient.SendMessage(new PipeMessage
                            {
                                Type = PipeMessage.MessageType.FirmwareUpdateStart,
                                Data = dataLength
                            });
                            // 等待信号，最多等待15秒
                            bool receivedSignal = waitEvent.WaitOne(15000);
                            if (!receivedSignal)
                            {
                                //等待超时，未收到信号
                                log.Error("未收到设备返回的固件更新开始消息！退出固件更新！");
                                Dispatcher.Invoke(() =>
                                {
                                    MessageBox.Show("未收到设备返回的固件更新开始消息！更新失败！", "更新失败", MessageBoxButton.OK, MessageBoxImage.Error);
                                    btnUpdateFirmware.Content = "更新固件";
                                });
                                return;
                            }

                            // 每次发送200字节的数据块
                            int totalBytesSent = 0;
                            int chunkSize = 200;

                            while (totalBytesSent < compressedData.Length)
                            {
                                int bytesToSend = Math.Min(chunkSize, compressedData.Length - totalBytesSent);
                                byte[] chunk = new byte[bytesToSend];
                                Array.Copy(compressedData, totalBytesSent, chunk, 0, bytesToSend);

                                pipeClient.SendMessage(new PipeMessage
                                {
                                    Type = PipeMessage.MessageType.FirmwareUpdateChunk,
                                    Data = chunk
                                });

                                totalBytesSent += bytesToSend;
                                receivedSignal = waitEvent.WaitOne(3000);
                                if (!receivedSignal)
                                {
                                    //等待超时，未收到信号
                                    log.Error("传输固件数据出错，长时间设备未响应。");
                                    Dispatcher.Invoke(() =>
                                    {
                                        MessageBox.Show("传输固件数据出错，长时间设备未响应！更新失败！", "更新失败", MessageBoxButton.OK, MessageBoxImage.Error);
                                        btnUpdateFirmware.Content = "更新固件";
                                    });
                                    return;
                                }

                                int progress = 20 + (totalBytesSent * 80 / compressedData.Length);
                                Dispatcher.Invoke(() =>
                                {
                                    btnUpdateFirmware.Content = "更新" + progress + "%";
                                });
                            }

                            log.Info($"[DOWNLOAD_COMPLETED]固件文件数据发送完成，开始发送结束命令");
                            pipeClient.SendMessage(new PipeMessage
                            {
                                Type = PipeMessage.MessageType.FirmwareUpdateEnd,
                                Data = Encoding.UTF8.GetBytes(crc32Value)
                            });
                            receivedSignal = waitEvent.WaitOne(10000);
                            if (!receivedSignal)
                            {
                                //等待超时，未收到信号
                                log.Error("长时间未收到设备更新成功消息，更新失败。");
                                Dispatcher.Invoke(() =>
                                {
                                    MessageBox.Show("长时间未收到设备更新成功消息！更新失败！", "更新失败", MessageBoxButton.OK, MessageBoxImage.Error);
                                    btnUpdateFirmware.Content = "更新固件";
                                });
                                return;
                            }
                            //
                            log.Info($"[DOWNLOAD_COMPLETED]固件文件发送完成！");
                           
                        }
                    }
                    catch (Exception ex)
                    {
                        log.Error($"传输固件更新时出错: {ex.Message}");
                    }
                   
                });
            }
        }

        private void FirmwareUpdater_DownloadProgress(object sender, System.ComponentModel.ProgressChangedEventArgs e)
        {
            log.Info($"{e.ProgressPercentage}% 下载完成 - {e.UserState}");
            Dispatcher.Invoke(() =>
            {
                btnUpdateFirmware.Content = "更新" + e.ProgressPercentage * 20/100 + "%";
            });
        }

        private void FirmwareUpdater_UpdateAvailable(object sender, UpdateInfo e)
        {
            log.Info($"发现新固件版本: {e.Version}");
            updateInfo = e;
            Dispatcher.Invoke(() =>
            {
                txtFirmwareVer.Text += "\t发现新版本：v" + e.Version;
                btnUpdateFirmware.Visibility = Visibility.Visible;
                btnUpdateFirmware.Content = "更新固件";
                btnUpdateFirmware.IsEnabled = true;
            });
        }

        #endregion

        #region 管道通信
        private void InitializePipeClient()
        {
            pipeClient = new PipeClient(this);
            StartShowWindowPipe();
        }
        
        private async void ConnectToService()
        {
            // 尝试连接到服务
            isConnectedToService = await pipeClient.ConnectAsync();
            
            if (isConnectedToService)
            {
                log.Info("[UI_PIPE]已连接到Sparkin服务");
                
                // 请求设备信息
                PipeMessage message = new PipeMessage
                {
                    Type = PipeMessage.MessageType.GetConnectState
                };
                pipeClient.SendMessage(message);

                // 请求设备配对状态
                message = new PipeMessage
                {
                    Type = PipeMessage.MessageType.BluetoothPairStatus
                };
                pipeClient.SendMessage(message);
            }
            else
            {
                log.Error("[UI_PIPE]无法连接到Sparkin服务");
                txtConnectionStatus.Text = "服务未运行";
                
                MessageBox.Show("无法连接到Sparkin服务，请确保服务已启动。", "连接失败", MessageBoxButton.OK, MessageBoxImage.Warning);
            }
        }
        
        // 消息处理委托
        public delegate bool ExternalMessageHandler(PipeMessage message);
        private List<ExternalMessageHandler> externalMessageHandlers = new List<ExternalMessageHandler>();
        
        /// <summary>
        /// 获取PipeClient实例，供其他窗口使用
        /// </summary>
        public PipeClient GetPipeClient()
        {
            return pipeClient;
        }
        
        /// <summary>
        /// 注册外部消息处理器
        /// </summary>
        public void RegisterExternalMessageHandler(ExternalMessageHandler handler)
        {
            if (handler != null && !externalMessageHandlers.Contains(handler))
            {
                externalMessageHandlers.Add(handler);
            }
        }
        
        /// <summary>
        /// 取消注册外部消息处理器
        /// </summary>
        public void UnregisterExternalMessageHandler(ExternalMessageHandler handler)
        {
            if (handler != null)
            {
                externalMessageHandlers.Remove(handler);
            }
        }
        
        public void OnMessageReceived(PipeMessage message)
        {
            Dispatcher.Invoke(() =>
            {
                try
                {
                    // 首先让外部消息处理器处理消息
                    bool handled = false;
                    foreach (var handler in externalMessageHandlers.ToArray())
                    {
                        if (handler(message))
                        {
                            handled = true;
                            break;
                        }
                    }
                    
                    // 如果未处理，则由主窗口自身处理
                    if (!handled)
                    {
                        switch (message.Type)
                        {
                            case PipeMessage.MessageType.BluetoothDeviceConnected:
                                { // 设备已连接
                                    string[] data = message.StringData.Split('|');
                                    if (data.Length == 2)
                                    {
                                        txtConnectionStatus.Text = "已连接";
                                        txtDeviceName.Text = data[0];
                                        UpdateBatteryDisplay(data[1] + "%");
                                        btnConnect.IsEnabled = true;
                                        cbSleepTime.IsEnabled = true;
                                        fingerItemsControl.IsEnabled = true;
                                        txtDeviceName.Text = data[0];
                                        btnConnect.Content = BTN_TEXT_UNPAIR;
                                        // 请求设备信息
                                        PipeMessage cmd = new PipeMessage
                                        {
                                            Type = PipeMessage.MessageType.GetDeviceInfo
                                        };
                                        pipeClient.SendMessage(cmd);
                                    }
                                }
                                break;
                            case PipeMessage.MessageType.BluetoothPair:
                                {// 开始执行配对操作
                                    Task.Run(async () =>
                                    {
                                        bool isPaired = false;
                                        if (string.IsNullOrEmpty(message.StringData))
                                        {
                                            isPaired = false;
                                        }
                                        else
                                        {
                                            isPaired = await bluetoothManager.UIPairDeviceAsync(message.StringData);
                                        }
                                        if (!isPaired)
                                        {
                                            Dispatcher.Invoke(() =>
                                            {
                                                // 更新UI状态   
                                                txtConnectionStatus.Text = "配对失败";
                                                //txtDeviceName.Text = "未知";
                                                txtFirmwareVer.Text = "未知";
                                                UpdateBatteryDisplay("--");
                                                btnConnect.IsEnabled = true;
                                                cbSleepTime.IsEnabled = false;
                                                fingerItemsControl.IsEnabled = false;
                                                MessageBox.Show("蓝牙设备配对失败！请开启电源后，长按底部配对按钮3秒，进入配对模式后再尝试连接。", "配对失败", MessageBoxButton.OK, MessageBoxImage.Error);
                                            });
                                        }
                                        else
                                        {
                                            // 请求设备信息
                                            PipeMessage msg = new PipeMessage
                                            {
                                                Type = PipeMessage.MessageType.GetConnectState
                                            };
                                            pipeClient.SendMessage(msg);
                                        }
                                    });
                                }
                                break;
                            case PipeMessage.MessageType.BluetoothPairStatus:
                                {
                                    string[] data = message.StringData.Split('|');
                                    if (data.Length == 3)
                                    {
                                        string deviceName = data[0];
                                        bool isPaired = bool.Parse(data[1]);
                                        string deviceId = data[2];
                                        if(isPaired)
                                        {
                                            Dispatcher.Invoke(() =>
                                            {
                                                txtDeviceName.Text = deviceName;
                                                btnConnect.Content = BTN_TEXT_UNPAIR;
                                                pairedDeviceId = deviceId;
                                            });
                                        }
                                        else
                                        {
                                            Dispatcher.Invoke(() =>
                                            {
                                                txtDeviceName.Text = "未知";
                                                btnConnect.Content = BTN_TEXT_PAIR;
                                                pairedDeviceId = "";
                                            });
                                        }
                                    }
                                }
                                break;
                            case PipeMessage.MessageType.BluetoothUnPair:
                                // 开始执行解除配对操作
                                Task.Run(async () =>
                                {
                                    bool unPaired = false;
                                    if (string.IsNullOrEmpty(message.StringData))
                                    {
                                        unPaired = true;
                                    }
                                    else
                                    {
                                        unPaired = await bluetoothManager.UIUnpairDeviceAsync(message.StringData);
                                    }
                                    if (unPaired)
                                    {
                                        Dispatcher.Invoke(() =>
                                        {
                                            // 更新UI状态   
                                            txtConnectionStatus.Text = "已解除配对";
                                            txtDeviceName.Text = "未知";
                                            txtFirmwareVer.Text = "未知";
                                            UpdateBatteryDisplay("--");
                                            btnConnect.IsEnabled = true;
                                            cbSleepTime.IsEnabled = false;
                                            fingerItemsControl.IsEnabled = false;
                                            btnConnect.Content = BTN_TEXT_PAIR;
                                            MessageBox.Show("蓝牙设备已成功解除配对！\r\n如果需要重新进行配对，请开启设备后，长按底部配对按钮3秒，进入配对模式后再配对。", "成功解除配对", MessageBoxButton.OK, MessageBoxImage.Information);
                                        });
                                    }
                                    else
                                    {
                                        Dispatcher.Invoke(() =>
                                        {
                                            // 显示错误信息
                                            MessageBox.Show("蓝牙设备解除配对失败！请检查设备是否正常！", "解除配对失败", MessageBoxButton.OK, MessageBoxImage.Error);
                                        });
                                    }
                                });
                                break;
                            case PipeMessage.MessageType.BluetoothDeviceDisconnected:
                                // 设备已断开
                                txtConnectionStatus.Text = "未连接";
                                //txtDeviceName.Text = "未知";
                                txtFirmwareVer.Text = "未知";
                                UpdateBatteryDisplay("--");
                                //btnConnect.IsEnabled = true;
                                cbSleepTime.IsEnabled = false;
                                fingerItemsControl.IsEnabled = false;
                                // 清空指纹控件
                                fingerItemsControl.Items.Clear();
                                btnUpdateFirmware.Visibility = Visibility.Hidden;
                                break;
                                
                            case PipeMessage.MessageType.BluetoothDataReceived:
                                // 处理蓝牙数据
                                ProcessBluetoothData(message.Data);
                                break;
                                
                            case PipeMessage.MessageType.BluetoothError:
                                // 处理错误
                                log.Error($"蓝牙错误: {message.StringData}");
                                MessageBox.Show("蓝牙设备连接失败！请尝试以下操作：\r\n1. 关闭设备电源。\r\n2. 点击“解除配对”按钮。\r\n3. 重新开机长按底部配对按钮3秒进入配对模式。\r\n4. 点击“配对设备”按钮进行配对即可。\r\n\r\n错误信息：" + message.StringData, "设备连接错误", MessageBoxButton.OK, MessageBoxImage.Error);
                                break;
                        }
                    }
                }
                catch (Exception ex)
                {
                    log.Error($"处理管道消息时出错: {ex.Message}");
                }
            });
        }
        
        private void ProcessBluetoothData(byte[] data)
        {
            if (data == null || data.Length < 4)
                return;
                
            DataHeader header = DataHeader.GetDataHeader(data);
            switch (header.cmd)
            {
                case CmdMessage.MSG_FINGERPRINT_DELETE:
                    log.Info("收到删除指纹返回结果：" + data[3]);
                    // 如果删除成功，请求更新指纹信息
                    if (data[3] == CmdMessage.MSG_CMD_SUCCESS)
                    {
                        SendGetFingerNamesRequest();
                    }
                    break;
                    
                case CmdMessage.MSG_SET_SLEEPTIME:
                    log.Info("收到设置睡眠时间请求结果：" + data[3]);
                    break;
                    
                case CmdMessage.MSG_FINGERPRINT_REGISTER_CANCEL:
                    log.Info("收到取消指纹注册请求结果：" + data[3]);
                    break;
                    
                case CmdMessage.MSG_GET_INFO:
                    MsgInfo msgInfo = StructConverter.ByteArrayToStructure<MsgInfo>(data, 3);
                    log.Info("睡眠时间：" + msgInfo.sleepTime);
                    log.Info("设备ID：" + msgInfo.deviceId);
                    log.Info("Build日期：" + msgInfo.buildDate);
                    log.Info("固件版本：" + msgInfo.firmwareVer);
                    
                    // 更新UI
                    cbSleepTime.SelectionChanged -= SleepTime_SelectionChanged;
                    cbSleepTime.SelectedValue = msgInfo.sleepTime;
                    cbSleepTime.SelectionChanged += SleepTime_SelectionChanged;
                    txtFirmwareVer.Text = "v" + msgInfo.firmwareVer;
                    SendGetFingerNamesRequest();
                    Task.Run(async() => { await firmwareUpdater.CheckForUpdateAsync(msgInfo.firmwareVer); });
                    break;
                    
                case CmdMessage.MSG_GET_FINGER_NAMES:
                    log.Info("收到获取所有指纹名称命令");
                    SendEnableSleepCommand(false); //不允许设备休眠
                    int offset = 4;
                    fingerprintCount = data[3];
                    
                    // 清空现有的指纹名称
                    fingerNames.Clear();
                    
                    // 解析指纹名称
                    for (int i = 0; i < fingerprintCount; i++)
                    {
                        FPData fpData = new FPData();
                        fpData.index = data[offset + i * 33];
                        fpData.fpName = Encoding.UTF8.GetString(data, offset + i * 33 + 1, CmdMessage.MAX_FINGER_NAME_LENGTH).TrimEnd('\0');
                        log.Info($"指纹ID: {fpData.index} 名称：{fpData.fpName}");
                        
                        // 保存指纹名称
                        fingerNames[fpData.index] = fpData.fpName;
                    }
                    
                    // 更新UI
                    UpdateFingerprintControls();
                    break;
                case CmdMessage.MSG_FIRMWARE_UPDATE_START:
                    log.Info("设备已经收到开始更新固件命令");
                    waitEvent.Set();
                    break;
                case CmdMessage.MSG_FIRMWARE_UPDATE_CHUNK:
                    //log.Info("设备已经收到固件数据");
                    waitEvent.Set();
                    break;
                case CmdMessage.MSG_FIRMWARE_UPDATE_END:
                    log.Info("设备已经收到固件更新结束命令");
                    waitEvent.Set();
                    Task.Run(() => { 
                        Dispatcher.Invoke(() =>
                        {
                            if (data[3] == CmdMessage.MSG_CMD_SUCCESS)
                            {
                                btnUpdateFirmware.Content = "更新完成";
                                MessageBox.Show("固件更新成功！设备已经重启。", "固件更新", MessageBoxButton.OK, MessageBoxImage.Information);
                            }
                            else
                            {
                                btnUpdateFirmware.Content = "更新失败";
                                btnUpdateFirmware.IsEnabled = true;
                                MessageBox.Show("固件更新失败！", "固件更新", MessageBoxButton.OK, MessageBoxImage.Error);
                            }
                        });
                    });
                    break;
                default:
                    log.Info($"收到未知命令：{header.cmd}");
                    log.Info($"数据：{BitConverter.ToString(data)}");
                    break;
            }
        }
        
        private void SendGetFingerNamesRequest()
        {
            if (isConnectedToService)
            {
                PipeMessage message = new PipeMessage
                {
                    Type = PipeMessage.MessageType.GetFingerNames
                };
                pipeClient.SendMessage(message);
            }
        }

        private void StartShowWindowPipe()
        {
            // 在新线程中启动管道服务器
            var thread = new Thread(RunShowWindowPipe)
            {
                IsBackground = true
            };
            thread.Start();
        }

        private void RunShowWindowPipe()
        {
            while (true)
            {
                try
                {
                    using (NamedPipeServerStream pipeServer = new NamedPipeServerStream("Sparkin-ShowWindow", PipeDirection.In))
                    {
                        // 等待客户端连接
                        pipeServer.WaitForConnection();

                        // 读取消息
                        using (var reader = new System.IO.StreamReader(pipeServer))
                        {
                            string message = reader.ReadLine();
                            if (message == "ShowWindow")
                            {
                                // 在UI线程中显示窗口
                                Dispatcher.Invoke(() =>
                                {
                                   ShowMainWindow();
                                });
                            }
                        }
                    }
                }
                catch (Exception ex)
                {
                    // 处理异常
                    log.Error($"Pipe server error: {ex.Message}");
                }
            }
        }
        #endregion

        #region 窗口响应事件

        private void Button_Click_TestUnlock(object sender, RoutedEventArgs e)
        {
            log.Info("[Test_Unlock]锁屏测试按钮按下");
            // 从界面输入框获取用户名和密码
            if (MessageBox.Show("点击“是(Y)”后，系统将锁屏，并在3秒后自动解锁。\r\n如果可以正常回到桌面，则用户名和密码正确。\r\n立即开始验证吗？","保存并验证密码",MessageBoxButton.YesNo,MessageBoxImage.Question) == MessageBoxResult.Yes)
            {
                ScreenUnlocker.LockScreen();
                Thread.Sleep(3000);
                ScreenUnlocker.Unlock(configFile.LoginUserName, configFile.LoginPassword, log);
            }
        }

        private void Button_Click_Connect(object sender, RoutedEventArgs e)
        {
            log.Info("[UI_Connect]配对按钮按下");
            if (btnConnect.Content.Equals(BTN_TEXT_PAIR))
            {
                // 清空消息显示区域
                txtConnectionStatus.Text = "搜索中...";
                // 通过服务开始搜索蓝牙设备
                if (isConnectedToService)
                {
                    btnConnect.IsEnabled = false;
                    PipeMessage message = new PipeMessage
                    {
                        Type = PipeMessage.MessageType.ConnectBluetooth
                    };
                    pipeClient.SendMessage(message);
                }
                else
                {
                    log.Error("[UI_Connect]未连接到服务，无法开始搜索蓝牙设备");
                    txtConnectionStatus.Text = "服务未运行";

                    // 尝试重新连接
                    ConnectToService();
                }
            }
            else
            {
                if(MessageBox.Show("确定要解除设备的配对吗？\r\n解除配对后，如果需要再次重新配对连接，需要长按设备底部配对按钮3秒以上，清除设备中配对信息后才可重新连接！", "解除配对", MessageBoxButton.YesNo, MessageBoxImage.Question) == MessageBoxResult.Yes)
                {
                    if (isConnectedToService)
                    {
                        btnConnect.IsEnabled = false;
                        PipeMessage message = new PipeMessage
                        {
                            Type = PipeMessage.MessageType.BluetoothUnPair
                        };
                        pipeClient.SendMessage(message);
                    }
                    else
                    {
                        log.Error("[UI_Connect]未连接到服务，无法解除配对！");
                        txtConnectionStatus.Text = "服务未运行";

                        // 尝试重新连接
                        ConnectToService();
                    }
                }
            }
        }

        private async void Button_Click_UpdateFirmware(object sender, RoutedEventArgs e)
        {
            log.Info("[UI_UpdateFirmware]更新固件按钮按下");
            if (updateInfo == null)
            {
                log.Error("[UI_UpdateFirmware]未找到更新信息");
                return;
            }
            string strInfo = $"最新版本：V{updateInfo.Version}\r\n更新内容：\r\n{updateInfo.Description}\r\n\r\n即将开始更新固件，请不要操作和关闭设备电源，确定开始吗？";
            if (MessageBox.Show(strInfo, "更新固件", MessageBoxButton.YesNo, MessageBoxImage.Question) == MessageBoxResult.Yes)
            {
                btnUpdateFirmware.Content = "正在更新...";
                btnUpdateFirmware.IsEnabled = false;
                await firmwareUpdater.DownloadUpdateAsync(updateInfo);
            }
        }

        private void SleepTime_SelectionChanged(object sender, System.Windows.Controls.SelectionChangedEventArgs e)
        {
            if (cbSleepTime.SelectedItem != null && isConnectedToService)
            {
                SleepTimeItem item = cbSleepTime.SelectedItem as SleepTimeItem;
                log.Info("[UI_SleepTime]设置睡眠时间：" + item.Value);
                
                // 通过服务设置睡眠时间
                PipeMessage message = new PipeMessage
                {
                    Type = PipeMessage.MessageType.SetSleepTime,
                    StringData = item.Value.ToString()
                };
                pipeClient.SendMessage(message);
            }
        }

        private void SendReloadConfigCommand()
        {
            if (isConnectedToService)
            {
                PipeMessage message = new PipeMessage
                {
                    Type = PipeMessage.MessageType.ReloadConfig,
                    StringData = ""
                };
                pipeClient.SendMessage(message);
            }
        }

        private void txtUsername_LostFocus(object sender, RoutedEventArgs e)
        {
            configFile.LoginUserName = txtUsername.Text;
            ConfigManager.SaveConfig(configFile, log);
            SendReloadConfigCommand();
        }

        private void txtPassword_LostFocus(object sender, RoutedEventArgs e)
        {
            configFile.LoginPassword = txtPassword.Password;
            ConfigManager.SaveConfig(configFile, log);
            SendReloadConfigCommand();
        }

        private void MainWindow_Closing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            if (!isClosing)
            {
                e.Cancel = true;
                Hide();
                return;
            }

            // 关闭管道连接
            if (pipeClient != null)
            {
                pipeClient.Disconnect();
            }

            // 移除托盘图标
            if (notifyIcon != null)
            {
                notifyIcon.Visible = false;
                notifyIcon.Dispose();
            }

        }

        private void SendEnableSleepCommand(bool bEnableSleep)
        {
            if (isConnectedToService)
            {
                PipeMessage message = new PipeMessage
                {
                    Type = PipeMessage.MessageType.SetEnableSleep,
                    Data = new byte[] { (byte)(bEnableSleep ? 1 : 0) }
                };
                pipeClient.SendMessage(message);
            }
        }

        private void MainWindow_IsVisibleChanged(object sender, DependencyPropertyChangedEventArgs e)
        {
            if (this.IsVisible)
            {
                log.Info("[VisibleChanged]窗口已经显示");
                // 窗口现在可见
                if (!isConnectedToService)
                {
                    // 如果管道未连接，重新连接
                    ConnectToService();
                }
                else
                {
                    // 已连接，只发送不允许设备休眠命令
                    SendEnableSleepCommand(false); //不允许设备休眠
                }
            }
            else
            {
                log.Info("[VisibleChanged]窗口已最小化");
                SendEnableSleepCommand(true);  //允许设备休眠
                // 窗口隐藏，断开管道连接
                if (pipeClient != null)
                {
                    pipeClient.Disconnect();
                    isConnectedToService = false;
                }
            }
        }
        #endregion

        #region 控件的更新和管理
        private void UpdateFingerprintControls()
        {
            // 清空现有控件
            fingerItemsControl.Items.Clear();

            // 添加已注册的指纹控件
            foreach (var item in fingerNames)
            {
                AddFingerprintControl(item.Key, item.Value);
            }

            // 添加"添加指纹"按钮（如果指纹数量未达到最大值）
            if (fingerprintCount < MAX_FINGER_NUM)
            {
                AddNewFingerprintButton();
            }
        }

        private void UpdateFingerprintLabels()
        {
            try
            {
                log.Info($"[UI_UpdateFingerprintLabels]更新指纹列表，当前指纹数量: {fingerprintCount}");
                
                if (fingerItemsControl.Items.Count > 0)
                {
                    foreach (var item in fingerItemsControl.Items)
                    {
                        try
                        {
                            if (item is StackPanel panel)
                            {
                                // 查找TextBlock
                                var textBlock = panel.Children.OfType<TextBlock>().FirstOrDefault();
                                if (textBlock != null)
                                {
                                    var tag = panel.Tag?.ToString();
                                    if (!string.IsNullOrEmpty(tag) && int.TryParse(tag, out int index) && fingerNames.ContainsKey(index))
                                    {
                                        textBlock.Text = fingerNames[index];
                                    }
                                }
                            }
                        }
                        catch (Exception ex)
                        {
                            log.Error($"[UI_UpdateFingerprintLabels]更新指纹标签时出错: {ex.Message}");
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                log.Error($"[UI_UpdateFingerprintLabels]更新指纹标签总错误: {ex.Message}");
            }
        }
        
        private void AddFingerprintControl(int index, string fingerName, bool newAdd = true)
        {
            StackPanel panel = new StackPanel();
            panel.Orientation = Orientation.Vertical;
            panel.Margin = new Thickness(5, 0, 5, 0);
            panel.Tag = index; // 保存指纹索引

            // 添加图像
            Image image = new Image();
            image.Height = 48;
            image.Width = 48;
            image.Margin = new Thickness(0, 5, 0, 0);
            image.Source = new BitmapImage(new Uri("/Images/finger.png", UriKind.Relative));


            // 创建删除按钮
            Image deleteButton = new Image();
            deleteButton.HorizontalAlignment = System.Windows.HorizontalAlignment.Right;
            deleteButton.VerticalAlignment = VerticalAlignment.Top;
            deleteButton.Margin = new Thickness(0, 0, 0, 0);
            deleteButton.Width = 14;
            deleteButton.Height = 14;
            deleteButton.Source = new BitmapImage(new Uri("/Images/btn_delete.png", UriKind.Relative));
            deleteButton.MouseLeftButtonDown += (s, e) => DeleteFingerprint(index); // 绑定删除事件
            deleteButton.Visibility = Visibility.Hidden;
            // 将图像和删除按钮添加到Grid中
            Grid imageGrid = new Grid();
            imageGrid.Children.Add(image);
            imageGrid.Children.Add(deleteButton);

            // 创建文本
            TextBlock textBlock = new TextBlock();
            // 如果存在指纹名称则使用，否则使用默认名称
            textBlock.Text = fingerName;
            textBlock.HorizontalAlignment = System.Windows.HorizontalAlignment.Center;
            textBlock.Name = "FingerPrintName" + index;
            textBlock.Cursor = Cursors.IBeam;
            textBlock.MouseLeftButtonDown += (s, e) => RenameFingerprint(index);

            // 注销可能存在的同名控件
            if (FindName(textBlock.Name) != null)
            {
                UnregisterName(textBlock.Name);
            }
            RegisterName(textBlock.Name, textBlock);

            // 添加到面板
            panel.Children.Add(imageGrid);
            panel.Children.Add(textBlock);
            // 创建右键菜单
            ContextMenu contextMenu = new ContextMenu();

            // 添加重命名选项
            MenuItem renameMenuItem = new MenuItem();
            renameMenuItem.Header = "重命名";
            renameMenuItem.Click += (s, e) => RenameFingerprint(index);
            contextMenu.Items.Add(renameMenuItem);

            // 添加删除选项
            MenuItem deleteMenuItem = new MenuItem();
            deleteMenuItem.Header = "删除";
            deleteMenuItem.Click += (s, e) => DeleteFingerprint(index);
            contextMenu.Items.Add(deleteMenuItem);

            // 设置上下文菜单
            panel.ContextMenu = contextMenu;
            panel.MouseRightButtonUp += (s, e) => panel.ContextMenu.IsOpen = true;

            // 鼠标进入事件
            panel.MouseEnter += (s, e) =>
            {
                deleteButton.Visibility = Visibility.Visible;
            };

            // 鼠标离开事件
            panel.MouseLeave += (s, e) =>
            {
                deleteButton.Visibility = Visibility.Hidden;
            };

            // 添加到控件集合
            fingerItemsControl.Items.Add(panel);
        }

        private void AddNewFingerprintButton()
        {
            StackPanel panel = new StackPanel();
            panel.Orientation = Orientation.Vertical;
            panel.Margin = new Thickness(5, 0, 5, 0);

            // 添加加号图像
            // 添加图像
            Image image = new Image();
            image.Height = 48;
            image.Width = 48;
            image.Margin = new Thickness(0, 5, 0, 0);
            image.Source = new BitmapImage(new Uri("/Images/plus-circle.png", UriKind.Relative));

            // 添加标签
            TextBlock textBlock = new TextBlock();
            textBlock.Text = "添加指纹";
            textBlock.HorizontalAlignment = System.Windows.HorizontalAlignment.Center;

            // 添加到面板
            panel.Children.Add(image);
            panel.Children.Add(textBlock);
            // 添加点击事件
            panel.MouseLeftButtonUp += (s, e) => OpenRegisterWindow();

            // 添加到控件集合
            fingerItemsControl.Items.Add(panel);
        }

            
        private byte FindAvailableFingerIndex()
        {
            for (byte i = 0; i < MAX_FINGER_NUM; i++)
            {
                if (!fingerNames.ContainsKey(i))
                {
                    return i;
                }
            }
            return (byte)MAX_FINGER_NUM;
        }

        private void RenameFingerprint(int index)
        {
            if (fingerNames.ContainsKey(index))
            {
                log.Info($"[UI_MENU]打开重命名指纹窗口，ID: {index}");
                RenameWindow renameWindow = new RenameWindow(fingerNames[index]);
                renameWindow.Owner = this;
                if (renameWindow.ShowDialog() == true)
                {
                    string newName = renameWindow.FingerName;
                    
                    // 通过服务发送重命名指纹命令
                    if (isConnectedToService)
                    {
                        PipeMessage message = new PipeMessage
                        {
                            Type = PipeMessage.MessageType.SetFingerName,
                            Data = new byte[] { (byte)index },
                            StringData = newName
                        };
                        pipeClient.SendMessage(message);
                        
                        // 更新本地指纹列表
                        fingerNames[index] = newName;
                        UpdateFingerprintLabels();
                    }
                }
            }
        }

        private void DeleteFingerprint(int index)
        {
            if (MessageBox.Show($"确定要删除指纹 \"{fingerNames[index]}\" 吗？", "删除确认", MessageBoxButton.YesNo, MessageBoxImage.Question) == MessageBoxResult.Yes)
            {
                SendDeleteFingerprintCommand(index);
            }
        }

        private void SendDeleteFingerprintCommand(int index)
        {
            if (isConnectedToService)
            {
                log.Info($"[UI_MENU]发送删除指纹命令，索引: {index}");
                
                PipeMessage message = new PipeMessage
                {
                    Type = PipeMessage.MessageType.SendBluetoothCommand,
                    Data = new byte[] { CmdMessage.MSG_FINGERPRINT_DELETE, (byte)index, 0x01 }
                };
                pipeClient.SendMessage(message);
                
                // 尝试立即更新UI，最终会在收到响应时再次更新
                if (fingerNames.ContainsKey(index))
                {
                    fingerNames.Remove(index);
                    fingerprintCount--;
                    UpdateFingerprintControls();
                }
            }
        }

        private void OpenRegisterWindow()
        {
            byte availableIndex = FindAvailableFingerIndex();
            if (availableIndex < MAX_FINGER_NUM)
            {
                RegisterWindow registerWindow = new RegisterWindow(availableIndex);
                registerWindow.Owner = this;
                if (registerWindow.ShowDialog() == true)
                {
                    // 更新本地指纹列表
                    fingerNames[availableIndex] = "指纹" + (availableIndex + 1).ToString();
                    fingerprintCount++;
                    UpdateFingerprintControls();
                }
            }
            else
            {
                MessageBox.Show("已达到最大指纹数量限制", "无法添加", MessageBoxButton.OK, MessageBoxImage.Warning);
            }
        }

        private void UpdateBatteryDisplay(string batteryLevel)
        {
            // 更新电量文本
            txtBattery.Text = batteryLevel;

            // 更新电池填充
            if (int.TryParse(batteryLevel.Replace("%", ""), out int batteryPercent))
            {
                // 计算填充宽度（总宽度32 - 边框2*2 - 内边距1*2 = 26）
                double maxWidth = 28; // 可填充的最大宽度
                double fillWidth = (maxWidth * batteryPercent) / 100;

                // 确保宽度不小于0且不大于最大宽度
                fillWidth = Math.Max(0, Math.Min(fillWidth, maxWidth));

                BatteryFill.Width = fillWidth;

                // 根据电量设置颜色
                if (batteryPercent <= 20)
                    BatteryFill.Fill = new SolidColorBrush(Color.FromRgb(239, 93, 109));
                else if (batteryPercent <= 50)
                    BatteryFill.Fill = new SolidColorBrush(Color.FromRgb(240, 233, 48));
                else
                    BatteryFill.Fill = new SolidColorBrush(Color.FromRgb(103, 250, 35));

                //// 更新电池边框颜色
                //var batteryIcon = (Border)FindName("BatteryIcon");
                //if (batteryIcon != null)
                //{
                //    if (batteryPercent <= 20)
                //        batteryIcon.BorderBrush = new SolidColorBrush(Colors.Red);
                //    else if (batteryPercent <= 50)
                //        batteryIcon.BorderBrush = new SolidColorBrush(Colors.Orange);
                //    else
                //        batteryIcon.BorderBrush = new SolidColorBrush(Colors.Black);
                //}
            }
            else
            {
                BatteryFill.Width = 0;
            }
        }
        #endregion

        #region 托盘图标
        private void InitializeTrayIcon()
        {
            // 创建托盘图标
            notifyIcon = new System.Windows.Forms.NotifyIcon();
            notifyIcon.Icon = System.Drawing.Icon.ExtractAssociatedIcon(System.Reflection.Assembly.GetExecutingAssembly().Location);
            notifyIcon.Text = "Sparkin指纹解锁";
            notifyIcon.Visible = true;
            
            // 双击托盘图标显示主窗口
            notifyIcon.MouseDoubleClick += (s, e) => ShowMainWindow();
            
            // 创建托盘菜单
            trayIconContextMenu = new ContextMenu();
            
            // 添加显示主窗口菜单项
            MenuItem showMenuItem = new MenuItem();
            showMenuItem.Header = "显示主界面";
            showMenuItem.Click += (s, e) => ShowMainWindow();
            trayIconContextMenu.Items.Add(showMenuItem);

            // 添加更新菜单项
            MenuItem updateMenuItem = new MenuItem();
            updateMenuItem.Header = "更新";
            updateMenuItem.Click += (s, e) => CheckUpdate();
            trayIconContextMenu.Items.Add(updateMenuItem);

            // 添加关于菜单项
            MenuItem aboutMenuItem = new MenuItem();
            aboutMenuItem.Header = "关于";
            aboutMenuItem.Click += (s, e) => ShowAboutWindow();
            trayIconContextMenu.Items.Add(aboutMenuItem);
            
            // 添加分隔符
            trayIconContextMenu.Items.Add(new Separator());
            
            // 添加退出菜单项
            MenuItem exitMenuItem = new MenuItem();
            exitMenuItem.Header = "退出";
            exitMenuItem.Click += (s, e) => {
                log.Info("[MENU]退出程序");
                isClosing = true;
                Close();
            };
            trayIconContextMenu.Items.Add(exitMenuItem);
            
            // 设置托盘图标的上下文菜单
            notifyIcon.ContextMenuStrip = new System.Windows.Forms.ContextMenuStrip();
            notifyIcon.ContextMenuStrip.Opening += (s, e) => {
                e.Cancel = true; // 取消默认菜单
                trayIconContextMenu.IsOpen = true; // 显示WPF菜单
            };
        }

        private void ShowAboutWindow()
        {
            log.Info("[MENU]打开关于窗口");
            AboutWindow aboutWindow = new AboutWindow();
            aboutWindow.ShowDialog();
        }
        
        private void ShowMainWindow()
        {
            log.Info("[MENU]打开主窗口");
            if (WindowState == WindowState.Minimized)
            {
                WindowState = WindowState.Normal;
            }
            
            Activate();
            Show();
            Focus();
        }

        private async void CheckUpdate()
        {
            log.Info("[MENU]检查更新");
            clientUpdater.UpdateUnavailable += ClientUpdater_UpdateUnavailable;
            Version version = Assembly.GetExecutingAssembly().GetName().Version;
            await clientUpdater.CheckForUpdateAsync($"{version.Major}.{version.Minor}");
        }
        
       
        #endregion

        private string Lang(string name)
        {
            try
            {
                // 尝试从资源字典获取本地化字符串
                var resource = (string)Application.Current.Resources[name];
                if (resource != null)
                {
                    return resource;
                }
            }
            catch
            {
                // 忽略异常
            }
            
            // 找不到资源时返回原始名称
            return name;
        }

    }
}

