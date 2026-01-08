using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
namespace SparkinLib.Bluetooth
{
    public class DataHeader
    {
        public byte cmd;
        public short dataLength;

        public static DataHeader GetDataHeader(byte[] data)
        {
            DataHeader header = new DataHeader();
            header.cmd = data[0];
            header.dataLength = BitConverter.ToInt16(data, 1);
            return header;
        }
    }
} 