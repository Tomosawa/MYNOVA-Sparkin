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
namespace SparkinLib
{
    [Serializable]
    public class PipeMessage
    {
        public enum MessageType
        {
            ConnectBluetooth,
            DisconnectBluetooth,
            SendBluetoothCommand,
            GetDeviceInfo,
            GetConnectState,
            BluetoothDeviceConnected,
            BluetoothDeviceDisconnected,
            BluetoothDataReceived,
            BluetoothError,
            BluetoothPair,
            BluetoothPairStatus,
            BluetoothUnPair,
            SetSleepTime,
            SetFingerName,
            GetFingerNames,
            UnlockScreen,
            GetLockScreenStatus,
            SetEnableSleep,
            ReloadConfig,
            FirmwareUpdateStart,
            FirmwareUpdateChunk,
            FirmwareUpdateEnd
        }

        public MessageType Type { get; set; }
        public byte[] Data { get; set; }
        public string StringData { get; set; }
    }

    public interface IPipeCallback
    {
        void OnMessageReceived(PipeMessage message);
    }
}
