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
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct FPData
    {
        public byte index;       // 指纹索引

        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)]
        public string fpName;   // 指纹名称
    }
} 