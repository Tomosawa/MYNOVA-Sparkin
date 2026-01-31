using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Threading.Tasks;
/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
namespace SparkinLib.Bluetooth
{
    [Serializable]
    public class BluetoothDeviceInfo 
    {
        private string _name;
        private string _id;
        private string _address;
        private bool _isPaired;
        private bool _canPair;
        private bool _isConnected;
        private int _signalStrength;
        private int _batteryLevel;
        private DateTime _lastSeen;

        public string Name
        {
            get => _name;
            set { _name = value; }
        }

        public string Id
        {
            get => _id;
            set { _id = value;  }
        }

        public string Address
        {
            get => _address;
            set { _address = value; }
        }

        public bool IsPaired
        {
            get => _isPaired;
            set { _isPaired = value;  }
        }
        /// <summary>
        /// 能否配对
        /// </summary>
        public bool CanPair
        {
            get { return _canPair; }
            set { _canPair = value; }
        }
        public bool IsConnected
        {
            get => _isConnected;
            set { _isConnected = value; }
        }

        public int SignalStrength
        {
            get => _signalStrength;
            set { _signalStrength = value; }
        }

        public int BatteryLevel
        {
            get => _batteryLevel;
            set { _batteryLevel = value; }
        }

        public DateTime LastSeen
        {
            get => _lastSeen;
            set { _lastSeen = value; }
        }

        public override string ToString()
        {
            return $"设备名称: {Name}, 地址: {Address}, 已配对: {IsPaired}, 已连接: {IsConnected}, 信号强度: {SignalStrength}";
        }
    }
} 