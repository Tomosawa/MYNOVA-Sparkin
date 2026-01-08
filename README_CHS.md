# MYNOVA-Sparkin

[English](README.md) | [中文文档](README_CHS.md)

## 项目概述

MYNOVA-Sparkin 是一款用于 Windows 计算机的无线蓝牙指纹识别设备。它提供快速指纹识别（0.5秒）、本地指纹管理和系统登录集成功能。该项目包含完整的硬件设计、固件和 Windows 配套软件。

![Sparkin Overview](Images/picture1.png)
![Sparkin Overview](Images/picture2.png)
![Sparkin Overview](Images/picture3.png)
![Sparkin PCB1](Images/PCB1.jpeg)
![Sparkin PCB2](Images/PCB2.jpeg)
![Sparkin Overview](Images/picture4.jpg)

## 主要特性

- **快速识别**：0.5 秒指纹识别速度
- **无线连接**：蓝牙 BLE 5.0 稳定连接
- **系统集成**：指纹验证通过后自动登录 Windows
- **本地存储**：指纹安全存储在设备上
- **便携设计**：内置电池，支持 Type-C 充电
- **省电模式**：自动睡眠模式节省电量
- **OTA 更新**：支持在线固件更新（需要有服务器）

## 快速开始

### 前置条件

- 支持蓝牙的 Windows 10/11 计算机
- Type-C 充电线（用于设备初始充电）

### 安装

1. 从 [Releases](https://github.com/yourusername/MYNOVA-Sparkin/releases) 页面下载最新的 Windows 安装程序
2. 运行安装程序并按照屏幕说明操作
3. 安装完成后，Sparkin 服务将自动启动
4. 从开始菜单启动 Sparkin 配置客户端

### 设备配对

1. 按住配对按钮 3 秒，直到 LED 闪烁
2. 打开 Sparkin 配置客户端
3. 点击"添加设备"并选择"Sparkin FP01"
4. 按照说明完成配对

### 指纹录入

1. 确保设备已连接
2. 在客户端中进入"指纹管理"
3. 点击"录入新指纹"
4. 按照指示多次扫描指纹
5. 为指纹分配名称

### 系统登录设置

1. 在客户端中进入"登录设置"
2. 启用"使用指纹自动登录"
3. 输入 Windows 登录密码
4. 点击"保存"完成设置

## 项目结构

```
MYNOVA-Sparkin/
├── SparkinFW/          # 设备固件 (Arduino/ESP32-C3)
├── SparkinWin/         # Windows 应用程序和安装文件
├── 3DModels/           # 外壳 3D 模型
├── Labels/             # 标签贴纸设计文件
└── PCB/                # PCB 设计文件
```

### SparkinFW
基于 Arduino 框架的 ESP32-C3 设备固件，包含：
- 指纹传感器管理
- 蓝牙 BLE 通信
- 电源和电池管理
- 睡眠模式控制
- OTA 更新支持

### SparkinWin
Windows 配套应用程序：
- 蓝牙通信后台服务
- 设备管理配置客户端
- 指纹录入界面
- 系统登录集成

## 开发

### 固件开发

- **IDE**: Arduino IDE 2.3.6+
- **ESP32 核心**: 3.1.1+
- **语言**: C++

### Windows 应用开发

- **IDE**: Visual Studio 2022
- **框架**: Windows 10/11 SDK
- **语言**: C#/C++

## 文档

详细技术文档：
- [固件架构](docs/firmware-architecture.md)
- [Windows 应用指南](docs/windows-application.md)

## 贡献

欢迎贡献！请遵循以下步骤：

1. Fork 仓库
2. 创建特性分支
3. 提交更改
4. 提交拉取请求

## 版权声明

本项目禁止用于任何商业用途、仅供学习和自己DIY使用。

## 许可证 [LICENSE](LICENSE)

本项目采用[GPLv3](LICENSE)许可证。[GPLv3（GNU General Public License version 3）](LICENSE)是一个自由、开源的软件许可证，它保证了用户运行、学习、分享和修改软件的自由。
完整的[GPLv3](LICENSE)许可证文本包含在本项目的[LICENSE](LICENSE)文件中。在使用、修改或分发本项目的代码之前，请确保你已阅读并理解了[GPLv3](LICENSE)许可证的全部内容。

## 致谢

- [Espressif Systems](https://www.espressif.com/) 的 ESP32-C3
- [Arduino 框架](https://www.arduino.cc/) 社区
- [Bluetooth SIG](https://www.bluetooth.com/) 的 BLE 标准
- [Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/secauthn/credential-providers-in-windows) 的 Windows 凭证提供程序文档

---


