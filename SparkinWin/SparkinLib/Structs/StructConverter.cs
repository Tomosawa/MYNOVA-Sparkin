using System;
using System.Runtime.InteropServices;
/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
namespace SparkinLib.Structs
{
    public static class StructConverter
    {
        // 从字节数组转换为结构体
        public static T ByteArrayToStructure<T>(byte[] bytes, int startIndex = 0) where T : struct
        {
            int size = Marshal.SizeOf(typeof(T));

            // 检查数据长度是否足够
            if (startIndex + size > bytes.Length)
            {
                throw new ArgumentException("数据长度不足", nameof(bytes));
            }

            IntPtr ptr = Marshal.AllocHGlobal(size);

            try
            {
                // 从startIndex开始复制字节到非托管内存
                Marshal.Copy(bytes, startIndex, ptr, size);
                return (T)Marshal.PtrToStructure(ptr, typeof(T));
            }
            finally
            {
                Marshal.FreeHGlobal(ptr);
            }
        }
    }
} 