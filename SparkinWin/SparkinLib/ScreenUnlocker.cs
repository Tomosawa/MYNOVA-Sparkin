using NLog;
using SparkinLib;
using System;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
public class ScreenUnlocker
{
 // 定义Win32 API导入
        [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Auto)]
        private static extern bool WaitNamedPipe(string lpNamedPipeName, uint nTimeOut);

        [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Auto)]
        private static extern IntPtr CreateFile(
            string lpFileName,
            uint dwDesiredAccess,
            uint dwShareMode,
            IntPtr lpSecurityAttributes,
            uint dwCreationDisposition,
            uint dwFlagsAndAttributes,
            IntPtr hTemplateFile);

        // 新增支持字节数组的WriteFile P/Invoke声明（替换原string版本）
        [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
        private static extern bool WriteFile(
            IntPtr hFile,
            byte[] lpBuffer,
            uint nNumberOfBytesToWrite,
            out uint lpNumberOfBytesWritten,
            IntPtr lpOverlapped);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool CloseHandle(IntPtr hObject);

        // 管道名称（需根据实际情况修改）
        private const string PipeName = "\\\\.\\pipe\\SparkinCredentialProvider";

        public static void Unlock(string username, string password, Logger log)
        {
            IntPtr pipeHandle = IntPtr.Zero;
            try
            {
                // 等待管道可用
                if (!WaitNamedPipe(PipeName, 5000))
                {
                    log.Info("无法打开管道");
                    return;
                }

                // 打开管道
                const uint GenericWrite = 0x40000000;    // GENERIC_WRITE
                const uint OpenExisting = 3;             // OPEN_EXISTING
                pipeHandle = CreateFile(
                    PipeName,
                    GenericWrite,
                    0,
                    IntPtr.Zero,
                    OpenExisting,
                    0,
                    IntPtr.Zero);

                if (pipeHandle == IntPtr.Zero || pipeHandle == new IntPtr(-1)) // 检查INVALID_HANDLE_VALUE
                {
                    log.Info($"打开管道失败，错误码：{Marshal.GetLastWin32Error()}");
                    return;
                }

                // 显式转换为UTF-16字节数组（包含终止符）
                byte[] usernameBytes = Encoding.Unicode.GetBytes(username + '\0');
                byte[] passwordBytes = Encoding.Unicode.GetBytes(password + '\0');

                // 发送用户名（使用字节数组版本的WriteFile）
                if (!WriteFile(pipeHandle, usernameBytes, (uint)usernameBytes.Length, out uint bytesWritten, IntPtr.Zero))
                {
                    log.Info($"写入用户名失败，错误码：{Marshal.GetLastWin32Error()}");
                    return;
                }

                // 发送密码
                if (!WriteFile(pipeHandle, passwordBytes, (uint)passwordBytes.Length, out bytesWritten, IntPtr.Zero))
                {
                    log.Info($"写入密码失败，错误码：{Marshal.GetLastWin32Error()}");
                    return;
                }
            }
            finally
            {
                // 确保关闭管道句柄
                if (pipeHandle != IntPtr.Zero && pipeHandle != new IntPtr(-1))
                {
                    CloseHandle(pipeHandle);
                }
            }
        }

        // 导入LockWorkStation函数
        [DllImport("user32.dll")]
        private static extern bool LockWorkStation();

        // 锁定屏幕
        public static bool LockScreen()
        {
            return LockWorkStation();
        }
}