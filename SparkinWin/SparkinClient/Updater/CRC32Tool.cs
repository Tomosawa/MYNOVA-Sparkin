using System.IO;
/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
public class CRC32Tool
{
    private const uint Polynomial = 0xEDB88320;
    private static readonly uint[] Table = new uint[256];

    static CRC32Tool()
    {
        // 初始化 CRC32 表
        for (uint i = 0; i < Table.Length; i++)
        {
            uint entry = i;
            for (int j = 0; j < 8; j++)
            {
                if ((entry & 1) == 1)
                {
                    entry = (entry >> 1) ^ Polynomial;
                }
                else
                {
                    entry >>= 1;
                }
            }
            Table[i] = entry;
        }
    }

    /// <summary>
    /// 计算文件的 CRC32 校验值
    /// </summary>
    /// <param name="filePath">文件路径</param>
    /// <returns>CRC32 校验值（十六进制字符串）</returns>
    public static string CalculateFileCrc32(string filePath)
    {
        if (!File.Exists(filePath))
        {
            return "";
        }

        uint crc = 0xFFFFFFFF;
        byte[] buffer = new byte[4096];

        using (FileStream stream = new FileStream(filePath, FileMode.Open, FileAccess.Read))
        {
            int bytesRead;
            while ((bytesRead = stream.Read(buffer, 0, buffer.Length)) > 0)
            {
                for (int i = 0; i < bytesRead; i++)
                {
                    crc = (crc >> 8) ^ Table[(crc & 0xFF) ^ buffer[i]];
                }
            }
            stream.Close();
        }

        // 取反并转换为十六进制字符串
        crc ^= 0xFFFFFFFF;
        return crc.ToString("X8");
    }
}

