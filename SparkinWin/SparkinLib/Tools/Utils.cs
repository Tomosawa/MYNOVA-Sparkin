using Microsoft.Win32;
using System;
using System.Runtime.InteropServices;
/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
namespace SparkinLib.Tools
{
    public class Utils
    {
        // 导入必要的Win32 API
        [DllImport("wtsapi32.dll")]
        private static extern bool WTSQuerySessionInformation(IntPtr hServer, int sessionId, WTS_INFO_CLASS wtsInfoClass, out IntPtr ppBuffer, out uint pBytesReturned);

        [DllImport("wtsapi32.dll")]
        private static extern void WTSFreeMemory(IntPtr pMemory);

        [DllImport("kernel32.dll")]
        private static extern int WTSGetActiveConsoleSessionId();

        [DllImport("user32.dll")]
        private static extern IntPtr OpenDesktop(string lpszDesktop, uint dwFlags, bool fInherit, uint dwDesiredAccess);

        [DllImport("user32.dll")]
        private static extern bool CloseDesktop(IntPtr hDesktop);

        [DllImport("user32.dll")]
        private static extern bool GetUserObjectInformation(IntPtr hObj, int nIndex, out IntPtr pvInfo, uint nLength, out uint lpnLengthNeeded);

        [DllImport("advapi32.dll", SetLastError = true)]
        private static extern bool OpenProcessToken(IntPtr ProcessHandle, uint DesiredAccess, out IntPtr TokenHandle);

        [DllImport("user32.dll")]
        private static extern bool GetShellWindow();

        // 常量定义
        private const uint DESKTOP_READOBJECTS = 0x0001;
        private const uint DESKTOP_SWITCHDESKTOP = 0x0100;
        private const int UOI_NAME = 2;

        private enum WTS_INFO_CLASS
        {
            WTSWinStationName = 2, // 查询当前会话的桌面名称
            WTSConnectState = 0,
            WTSInfoClassEnd = 29
        }

        public static bool IsSystemLocked()
        {
            try
            {
                int sessionId = WTSGetActiveConsoleSessionId();
                if (sessionId == -1) // INVALID_SESSION_ID
                    return false;

                IntPtr buffer;
                uint bytesReturned;

                // 查询当前会话的桌面名称
                if (WTSQuerySessionInformation(IntPtr.Zero, sessionId, WTS_INFO_CLASS.WTSWinStationName, out buffer, out bytesReturned))
                {
                    try
                    {
                        string stationName = Marshal.PtrToStringAnsi(buffer);

                        // 如果是Winlogon桌面，表示系统被锁定
                        if (stationName == "Winlogon")
                            return true;

                        // 尝试打开桌面并检查是否可以切换
                        IntPtr hDesktop = OpenDesktop("Default", 0, false, DESKTOP_READOBJECTS | DESKTOP_SWITCHDESKTOP);
                        if (hDesktop == IntPtr.Zero)
                        {
                            // 无法打开Default桌面，可能被锁定
                            return true;
                        }

                        try
                        {
                            // 获取桌面名称进行验证
                            IntPtr namePtr;
                            uint lengthNeeded;
                            if (GetUserObjectInformation(hDesktop, UOI_NAME, out namePtr, 0, out lengthNeeded))
                            {
                                string desktopName = Marshal.PtrToStringAnsi(namePtr);
                                return desktopName == "Winlogon";
                            }
                        }
                        finally
                        {
                            CloseDesktop(hDesktop);
                        }
                    }
                    finally
                    {
                        WTSFreeMemory(buffer);
                    }
                }
            }
            catch
            {
                // 发生异常时，默认返回false（未锁定）
            }

            return false;
        }
    }
}
