using System;
using System.IO;
using System.IO.Pipes;
using System.Runtime.Serialization.Formatters.Binary;
using System.Threading;
using System.Security.AccessControl;
using System.Security.Principal;
using NLog;
/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
namespace SparkinLib
{
    public class PipeServer
    {
        private const string PipeName = "\\\\.\\pipe\\SparkinService";
        private NamedPipeServerStream serverPipe;
        private volatile bool isRunning = false;
        private Thread serverThread;
        private readonly IPipeCallback callback;
        private readonly object serverLock = new object();
        private CancellationTokenSource cancellationSource;

        // 日志记录
        private Logger log = LogUtil.GetLogger();

        public PipeServer(IPipeCallback callback)
        {
            this.callback = callback;
        }

        public void Start()
        {
            if (isRunning)
                return;

            lock (serverLock)
            {
                if (isRunning)
                    return;

                isRunning = true;
                cancellationSource = new CancellationTokenSource();
                serverThread = new Thread(ServerLoop);
                serverThread.IsBackground = true;
                serverThread.Start();
                
                log.Info("[PIPE_SERVER]管道服务器已启动");
            }
        }

        public void Stop()
        {
            if (!isRunning)
                return;

            lock (serverLock)
            {
                if (!isRunning)
                    return;

                isRunning = false;
                cancellationSource?.Cancel();
                
                CloseServerPipe();
                
                serverThread?.Join(1000);
                serverThread = null;
                
                log.Info("[PIPE_SERVER]管道服务器已停止");
            }
        }

        public bool IsConnected()
        {
            lock (serverLock)
            {
                return serverPipe != null && serverPipe.IsConnected;
            }
        }

        private void CloseServerPipe()
        {
            lock (serverLock)
            {
                if (serverPipe != null)
                {
                    try
                    {
                        if (serverPipe.IsConnected)
                            serverPipe.Disconnect();
                        
                        serverPipe.Dispose();
                    }
                    catch (Exception ex)
                    {
                        log.Error($"[PIPE_SERVER]关闭管道错误: {ex.Message}");
                    }
                    finally
                    {
                        serverPipe = null;
                    }
                }
            }
        }

        private void ServerLoop()
        {
            while (isRunning)
            {
                CancellationToken token = cancellationSource.Token;
                
                try
                {
                    CreateServerPipe();
                    
                    log.Info("[PIPE_SERVER]等待客户端连接...");
                    serverPipe.WaitForConnection();
                    log.Info("[PIPE_SERVER]客户端已连接，开始处理数据");

                    // 处理与客户端的通信，直到客户端断开或服务停止
                    HandleClientConnection(token);

                    log.Info("[PIPE_SERVER]客户端已断开连接");
                }
                catch (OperationCanceledException)
                {
                    // 正常取消，不需要记录错误
                    break;
                }
                catch (Exception ex)
                {
                    log.Error($"[PIPE_SERVER]管道服务器错误: {ex.Message}");
                    Thread.Sleep(1000); // 等待一段时间再重试
                }
                finally
                {
                    // 确保管道被关闭，准备下一次连接
                    log.Info("[PIPE_SERVER]管道被关闭");
                    CloseServerPipe();
                }
            }
        }
        
        private void CreateServerPipe()
        {
            lock (serverLock)
            {
                if (serverPipe != null)
                {
                    CloseServerPipe();
                }
                
                // 创建允许任何用户访问的安全描述符
                PipeSecurity pipeSecurity = new PipeSecurity();
                SecurityIdentifier everyone = new SecurityIdentifier(WellKnownSidType.WorldSid, null);
                PipeAccessRule rule = new PipeAccessRule(everyone, 
                                                        PipeAccessRights.ReadWrite,
                                                        AccessControlType.Allow);
                pipeSecurity.AddAccessRule(rule);

                // 创建命名管道实例
                serverPipe = new NamedPipeServerStream(
                    PipeName,
                    PipeDirection.InOut,
                    NamedPipeServerStream.MaxAllowedServerInstances,
                    PipeTransmissionMode.Message,
                    PipeOptions.Asynchronous,
                    0, 0,
                    pipeSecurity);
            }
        }

        private void HandleClientConnection(CancellationToken token)
        {
            if (serverPipe == null || !serverPipe.IsConnected)
                return;

            byte[] lengthBytes = new byte[4];
            byte[] messageBytes = null;

            while (!token.IsCancellationRequested && serverPipe != null && serverPipe.IsConnected)
            {
                try
                {
                    // 接收消息长度
                    int bytesRead = 0;
                    int totalBytesRead = 0;
                    
                    while (totalBytesRead < 4)
                    {
                        bytesRead = serverPipe.Read(lengthBytes, totalBytesRead, 4 - totalBytesRead);
                        if (bytesRead == 0)
                        {
                            // 连接已关闭
                            return;
                        }
                        totalBytesRead += bytesRead;
                    }
                    
                    int length = BitConverter.ToInt32(lengthBytes, 0);
                    
                    // 验证消息长度是否合理，防止恶意消息
                    if (length <= 0 || length > 1024 * 1024) // 限制最大1MB
                    {
                        log.Error($"[PIPE_SERVER]接收到无效消息长度: {length}");
                        return;
                    }

                    // 接收完整消息
                    messageBytes = new byte[length];
                    totalBytesRead = 0;
                    
                    while (totalBytesRead < length)
                    {
                        bytesRead = serverPipe.Read(messageBytes, totalBytesRead, length - totalBytesRead);
                        if (bytesRead == 0)
                        {
                            // 连接已关闭
                            return;
                        }
                        totalBytesRead += bytesRead;
                    }

                    // 反序列化并处理消息
                    PipeMessage message = DeserializeMessage(messageBytes);
                    if (message != null)
                    {
                        callback?.OnMessageReceived(message);
                    }
                }
                catch (IOException ex)
                {
                    // 管道可能断开连接
                    log.Error($"[PIPE_SERVER]管道IO错误: {ex.Message}");
                    return;
                }
                catch (Exception ex)
                {
                    log.Error($"[PIPE_SERVER]处理客户端消息错误: {ex.Message}");
                    return;
                }
            }
        }
        
        private PipeMessage DeserializeMessage(byte[] messageBytes)
        {
            try
            {
                using (MemoryStream ms = new MemoryStream(messageBytes))
                {
                    BinaryFormatter formatter = new BinaryFormatter();
                    return (PipeMessage)formatter.Deserialize(ms);
                }
            }
            catch (Exception ex)
            {
                log.Error($"[PIPE_SERVER]消息反序列化错误: {ex.Message}");
                return null;
            }
        }

        public void SendMessage(PipeMessage message)
        {
            lock (serverLock)
            {
                if (serverPipe == null || !serverPipe.IsConnected)
                    return;

                try
                {
                    // 序列化消息
                    byte[] messageBytes = SerializeMessage(message);
                    if (messageBytes == null)
                        return;

                    // 发送长度
                    byte[] lengthBytes = BitConverter.GetBytes(messageBytes.Length);
                    serverPipe.Write(lengthBytes, 0, lengthBytes.Length);

                    // 发送消息
                    serverPipe.Write(messageBytes, 0, messageBytes.Length);
                    serverPipe.Flush();
                }
                catch (Exception ex)
                {
                    log.Error($"[PIPE_SERVER]发送消息错误: {ex.Message}");
                }
            }
        }
        
        private byte[] SerializeMessage(PipeMessage message)
        {
            try
            {
                using (MemoryStream ms = new MemoryStream())
                {
                    BinaryFormatter formatter = new BinaryFormatter();
                    formatter.Serialize(ms, message);
                    return ms.ToArray();
                }
            }
            catch (Exception ex)
            {
                log.Error($"[PIPE_SERVER]消息序列化错误: {ex.Message}");
                return null;
            }
        }
    }
} 