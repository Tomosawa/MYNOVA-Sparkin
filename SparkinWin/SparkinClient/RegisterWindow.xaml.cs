using System;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Threading;
using SparkinLib;
using SparkinLib.Bluetooth;
using XamlAnimatedGif;
/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
namespace SparkinClient
{
    /// <summary>
    /// RegisterWindow.xaml 的交互逻辑
    /// </summary>
    public partial class RegisterWindow 
    {
        private PipeClient pipeClient;
        private byte fingerId;
        private int currentStep = 0;
        private int totalStep = 5;
        private string[] tipInfo = new string[5];
        private string tipFailed = "录入失败！请拿开手指，重新放上去试试看，保持稳定。";

        private DispatcherTimer autoCloseTimer = new DispatcherTimer();
        private int timerCountDown = 4;

        private bool bFailed = false; // 是否录入失败一次

        public RegisterWindow(byte fingerId)
        {
            this.fingerId = fingerId;
            InitializeComponent();
            
            // 获取MainWindow中的管道客户端实例
            if (Application.Current.MainWindow is MainWindow mainWindow)
            {
                // 使用MainWindow中已有的管道客户端连接
                pipeClient = ((MainWindow)Application.Current.MainWindow).GetPipeClient();
            }

            // 文字提示初始化
            tipInfo[0] = "开始第一次指纹录入，请将手指放在指纹传感器上，保持稳定几秒即可。";
            tipInfo[1] = "第二次录入开始，请将手指移开后再次放入，可适当调整手指角度，使录入更全面。";
            tipInfo[2] = "进行第三次录入，请移开后再次将手指放在传感器上，尽量覆盖中心区域，过程很快完成。";
            tipInfo[3] = "第四次录入开始，请移开手指，调整角度重新放上，让指纹所有位置都被捕捉。";
            tipInfo[4] = "最后一次录入，请将指尖部位放在传感器上，完成后指纹录入即成功，感谢配合。";

            // 窗口加载完成后发送注册指纹命令
            Loaded += RegisterWindow_Loaded;
            
            // 窗口关闭时清理事件
            Closed += RegisterWindow_Closed;

            // 定时器初始化
            autoCloseTimer.Interval = TimeSpan.FromSeconds(1);
            autoCloseTimer.Tick += AutoCloseTimer_Tick;
        }

        private void AutoCloseTimer_Tick(object sender, EventArgs e)
        {
            if (timerCountDown <= 0)
            {
                autoCloseTimer.Stop();
                BtnConfirm_Click(sender, null);
                return;
            }
            btnConfirm.Content = $"确认（{--timerCountDown}）";
        }

        private void RegisterWindow_Loaded(object sender, RoutedEventArgs e)
        {
            // 获取MainWindow中当前正在处理的管道消息并设置处理函数
            if (Application.Current.MainWindow is MainWindow mainWindow)
            {
                mainWindow.RegisterExternalMessageHandler(HandlePipeMessage);
            }
            
            // 发送注册指纹命令
            SendRegisterFingerprintCommand();
        }
        
        private void RegisterWindow_Closed(object sender, EventArgs e)
        {
            // 取消消息处理
            if (Application.Current.MainWindow is MainWindow mainWindow)
            {
                mainWindow.UnregisterExternalMessageHandler(HandlePipeMessage);
            }
            
            // 如果窗口被关闭但没有点击确认按钮，发送取消注册命令
            if (DialogResult != true)
            {
                SendCancelRegisterCommand();
            }
            
            if (autoCloseTimer.IsEnabled)
                autoCloseTimer.Stop();
        }

        private void BtnCancel_Click(object sender, RoutedEventArgs e)
        {
            // 设置对话框结果为false（取消）
            if(autoCloseTimer.IsEnabled)
                autoCloseTimer.Stop();
            DialogResult = false;
            Close();
        }

        private void BtnConfirm_Click(object sender, RoutedEventArgs e)
        {
            // 设置对话框结果为true（确认）
            if (autoCloseTimer.IsEnabled)
                autoCloseTimer.Stop();
            
            DialogResult = true;
            Close();
        }

        private bool HandlePipeMessage(PipeMessage message)
        {
            if (message.Type != PipeMessage.MessageType.BluetoothDataReceived || message.Data == null || message.Data.Length < 4)
                return false;
                
            byte[] data = message.Data;
            DataHeader header = DataHeader.GetDataHeader(data);
            
            switch (header.cmd)
            {
                case CmdMessage.MSG_PUT_FINGER:
                    {
                        byte result = data[3];
                        if (result == CmdMessage.MSG_CMD_EXECUTE)
                        {
                            // 判断是第一次收到，则设置文字
                            if(currentStep == 0)
                            {
                                txtStatus.Text = tipInfo[currentStep];
                                pbarStep.Value = 0;
                            }
                        }
                        else if (result == CmdMessage.MSG_CMD_SUCCESS)
                        {
                            currentStep++;
                            if(currentStep >= tipInfo.Length)
                                currentStep = tipInfo.Length - 1;
                            txtStatus.Text = tipInfo[currentStep];
                            txtStep.Text = (currentStep).ToString() + "/" + totalStep.ToString();
                            pbarStep.Value = currentStep;
                            if (bFailed)
                            {
                                AnimationBehavior.SetSourceUri(imgCtrl, new Uri("pack://application:,,,/Images/scanfinger.gif"));
                                bFailed = false;
                            }
                        }
                        else if(result == CmdMessage.MSG_CMD_FAILURE)
                        {
                            txtStatus.Text = tipFailed;
                            AnimationBehavior.SetSourceUri(imgCtrl,new Uri("pack://application:,,,/Images/scanfinger_failed.gif"));
                            bFailed = true;
                        }
                        return true;
                    }
                    
                case CmdMessage.MSG_REMOVE_FINGER:
                    return true;
                    
                case CmdMessage.MSG_FINGERPRINT_REGISTER:
                    {
                        // 检查指纹注册结果
                        byte result = data[3];
                        if (result == CmdMessage.MSG_CMD_SUCCESS)
                        {
                            txtStatus.Text = "指纹注册成功！";
                            txtStep.Text = "5/5";
                            pbarStep.Value = 5;
                            btnConfirm.IsEnabled = true;
                            btnCancel.Visibility = Visibility.Collapsed;
                            btnConfirm.Visibility = Visibility.Visible;
                            autoCloseTimer.Start();
                        }
                        else
                        {
                            txtStatus.Text = "指纹注册失败，请返回重试";
                        }
                        return true;
                    }
            }
            
            return false;
        }
        
        private void SendRegisterFingerprintCommand()
        {
            try
            {
                txtStatus.Text = "正在初始化指纹注册...";
                
                // 发送注册指纹命令
                if (pipeClient != null)
                {
                    // 使用管道通信发送注册命令
                    byte[] commandData = new byte[2];
                    commandData[0] = CmdMessage.MSG_FINGERPRINT_REGISTER;
                    commandData[1] = fingerId;
                    
                    PipeMessage message = new PipeMessage
                    {
                        Type = PipeMessage.MessageType.SendBluetoothCommand,
                        Data = commandData
                    };
                    
                    pipeClient.SendMessage(message);
                }
                else
                {
                    txtStatus.Text = "服务未连接，无法注册指纹";
                }
            }
            catch (Exception ex)
            {
                txtStatus.Text = "发送注册命令时出错";
                MessageBox.Show($"发送注册指纹命令时出错: {ex.Message}", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }
        
        private void SendCancelRegisterCommand()
        {
            try
            {
                if (pipeClient != null)
                {
                    // 使用管道通信发送取消注册命令
                    byte[] commandData = new byte[2];
                    commandData[0] = CmdMessage.MSG_FINGERPRINT_REGISTER_CANCEL;
                    commandData[1] = 0x01;
                    
                    PipeMessage message = new PipeMessage
                    {
                        Type = PipeMessage.MessageType.SendBluetoothCommand,
                        Data = commandData
                    };
                    
                    pipeClient.SendMessage(message);
                }
            }
            catch
            {
                // 忽略错误，因为窗口已关闭
            }
        }
    }
}
