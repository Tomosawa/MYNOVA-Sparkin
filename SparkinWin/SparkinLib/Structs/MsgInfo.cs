using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
namespace SparkinLib.Structs
{
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct MsgInfo
    {
        public uint sleepTime;    // 睡眠时间设定

        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 20)]
        public string deviceId;

        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 10)]
        public string buildDate;

        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 10)]
        public string firmwareVer;
    }
} 