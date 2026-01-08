using NLog;
using System;
using System.Collections.Generic;
using System.IO.Pipes;
using System.IO;
using System.Linq;
using System.Runtime.Serialization.Formatters.Binary;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
namespace SparkinLib
{
    public class PipeClient
    {
        private const string PipeName = "\\\\.\\pipe\\SparkinService";
        private NamedPipeClientStream clientPipe;
        private volatile bool isConnected = false;
        private Thread receiveThread;
        private readonly IPipeCallback callback;
        private readonly object clientLock = new object();
        private CancellationTokenSource cancellationSource;

        public bool IsConnected => isConnected;

        // 日志记录
        private Logger log = LogUtil.GetLogger();

        public PipeClient(IPipeCallback callback)
        {
            this.callback = callback;
        }

        public async Task<bool> ConnectAsync(int timeoutMs = 5000)
        {
            lock (clientLock)
            {
                if (isConnected)
                    return true;

                // 如果之前有连接，确保已经完全清理
                Disconnect();
            }

            try
            {
                // 创建取消标记
                cancellationSource = new CancellationTokenSource();

                // 创建客户端管道
                clientPipe = new NamedPipeClientStream(".", PipeName, PipeDirection.InOut, PipeOptions.Asynchronous);

                // 尝试连接
                await clientPipe.ConnectAsync(timeoutMs);

                lock (clientLock)
                {
                    isConnected = true;

                    // 启动接收线程
                    receiveThread = new Thread(ReceiveMessages);
                    receiveThread.IsBackground = true;
                    receiveThread.Start(cancellationSource.Token);
                }

                log.Info("[PIPE_CLIENT]已连接到管道服务器");
                return true;
            }
            catch (Exception ex)
            {
                log.Error($"[PIPE_CLIENT]连接管道服务器失败: {ex.Message}");
                Disconnect();
                return false;
            }
        }

        public void Disconnect()
        {
            lock (clientLock)
            {
                isConnected = false;
                cancellationSource?.Cancel();

                if (clientPipe != null)
                {
                    try
                    {
                        clientPipe.Close();
                        clientPipe.Dispose();
                    }
                    catch (Exception ex)
                    {
                        log.Error($"[PIPE_CLIENT]关闭客户端管道错误: {ex.Message}");
                    }
                    finally
                    {
                        clientPipe = null;
                    }
                }

                if (receiveThread != null && receiveThread.IsAlive)
                {
                    receiveThread.Join(1000);
                    receiveThread = null;
                }
            }

            log.Info("[PIPE_CLIENT]已断开与管道服务器的连接");
        }

        private void ReceiveMessages(object tokenObj)
        {
            CancellationToken token = (CancellationToken)tokenObj;

            byte[] lengthBytes = new byte[4];
            byte[] messageBytes = null;

            try
            {
                while (!token.IsCancellationRequested && isConnected && clientPipe != null && clientPipe.IsConnected)
                {
                    try
                    {
                        // 接收消息长度
                        int bytesRead = 0;
                        int totalBytesRead = 0;

                        while (totalBytesRead < 4)
                        {
                            bytesRead = clientPipe.Read(lengthBytes, totalBytesRead, 4 - totalBytesRead);
                            if (bytesRead == 0)
                            {
                                // 连接已关闭
                                isConnected = false;
                                return;
                            }
                            totalBytesRead += bytesRead;
                        }

                        int length = BitConverter.ToInt32(lengthBytes, 0);

                        // 验证消息长度是否合理，防止恶意消息
                        if (length <= 0 || length > 1024 * 1024) // 限制最大1MB
                        {
                            log.Error($"[PIPE_CLIENT]接收到无效消息长度: {length}");
                            isConnected = false;
                            return;
                        }

                        // 接收完整消息
                        messageBytes = new byte[length];
                        totalBytesRead = 0;

                        while (totalBytesRead < length)
                        {
                            bytesRead = clientPipe.Read(messageBytes, totalBytesRead, length - totalBytesRead);
                            if (bytesRead == 0)
                            {
                                // 连接已关闭
                                isConnected = false;
                                return;
                            }
                            totalBytesRead += bytesRead;
                        }

                        // 反序列化
                        using (MemoryStream ms = new MemoryStream(messageBytes))
                        {
                            BinaryFormatter formatter = new BinaryFormatter();
                            PipeMessage message = (PipeMessage)formatter.Deserialize(ms);
                            callback?.OnMessageReceived(message);
                        }
                    }
                    catch (IOException ex)
                    {
                        // 管道可能断开连接
                        log.Error($"[PIPE_CLIENT]管道IO错误: {ex.Message}");
                        isConnected = false;
                        return;
                    }
                    catch (Exception ex)
                    {
                        log.Error($"[PIPE_CLIENT]接收消息错误: {ex.Message}");
                        // 继续尝试接收其他消息
                    }
                }
            }
            catch (Exception ex)
            {
                log.Error($"[PIPE_CLIENT]接收线程异常: {ex.Message}");
            }
            finally
            {
                isConnected = false;
            }
        }

        public void SendMessage(PipeMessage message)
        {
            lock (clientLock)
            {
                if (clientPipe == null || !isConnected)
                    return;

                try
                {
                    // 序列化消息
                    byte[] messageBytes;
                    using (MemoryStream ms = new MemoryStream())
                    {
                        BinaryFormatter formatter = new BinaryFormatter();
                        formatter.Serialize(ms, message);
                        messageBytes = ms.ToArray();
                    }

                    // 发送长度
                    byte[] lengthBytes = BitConverter.GetBytes(messageBytes.Length);
                    clientPipe.Write(lengthBytes, 0, lengthBytes.Length);

                    // 发送消息
                    clientPipe.Write(messageBytes, 0, messageBytes.Length);
                    clientPipe.Flush();
                }
                catch (Exception ex)
                {
                    log.Error($"[PIPE_CLIENT]发送消息错误: {ex.Message}");
                    isConnected = false;
                }
            }
        }
    }
}
