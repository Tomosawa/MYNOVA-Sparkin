using System;
using System.Text;
using System.Runtime.InteropServices;
/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
namespace SparkinLib.Bluetooth
{
    /// <summary>
    /// 蓝牙命令类型常量
    /// </summary>
    public static class CmdMessage
    {
        public const byte MSG_FINGERPRINT_SEARCH = 0x01;  // 指纹识别成功
        public const byte MSG_FINGERPRINT_REGISTER = 0x02;  // 指纹注册请求
        public const byte MSG_FINGERPRINT_DELETE = 0x03;    // 指纹删除请求
        public const byte MSG_DEVICE_NOTIFY = 0x04;  // 订阅成功的通知
        public const byte MSG_LOCKSCREEN_STATUS = 0x05;  // 锁屏状态
        public const byte MSG_PUT_FINGER = 0x06;  // 放入手指命令
        public const byte MSG_REMOVE_FINGER = 0x07;  // 移除手指命令
        public const byte MSG_GET_INFO = 0x08;  // 获取设备信息
        public const byte MSG_FINGERPRINT_REGISTER_CANCEL = 0x09;  // 取消指纹注册请求
        public const byte MSG_SET_SLEEPTIME = 0x10; //设置睡眠时间
        public const byte MSG_SET_FINGER_NAME = 0x20; //设置指纹名称
        public const byte MSG_GET_FINGER_NAMES = 0x21; //获取所有指纹名称
        public const byte MSG_RENAME_FINGER_NAME = 0x22; //重命名指纹名称
        public const byte MSG_ENALBE_SLEEP = 0x23; //是否允许设备休眠
        public const byte MSG_FIRMWARE_UPDATE_START = 0x24; //固件升级开始
        public const byte MSG_FIRMWARE_UPDATE_CHUNK = 0x25; //固件升级传输
        public const byte MSG_FIRMWARE_UPDATE_END = 0x26; //固件升级结束
        public const byte MSG_CHECK_SLEEP = 0x27; // 检查可否现在进行休眠，返回UI界面是否打开的状态

        public const byte MAX_FINGER_NAME_LENGTH = 32; //指纹名称最大长度

        public const byte MSG_CMD_SUCCESS = 0xA1; //命令执行成功
        public const byte MSG_CMD_FAILURE = 0xA0; //命令执行失败
        public const byte MSG_CMD_EXECUTE = 0xA2; //命令执行中
        public const byte MSG_CMD_CANCEL = 0xA3; //命令取消
    }
} 