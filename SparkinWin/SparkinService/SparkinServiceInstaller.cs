using System.ComponentModel;
using System.ServiceProcess;
/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
namespace SparkinService
{
    [RunInstaller(true)]
    public class SparkinServiceInstaller : System.Configuration.Install.Installer
    {
        ServiceProcessInstaller serviceProcessInstaller;
        ServiceInstaller serviceInstaller;
        public SparkinServiceInstaller()
        {
            serviceProcessInstaller = new ServiceProcessInstaller{
                Account = ServiceAccount.LocalSystem,
                Username = null,
                Password = null
            };
            serviceInstaller = new ServiceInstaller{
                ServiceName = "SparkinService",
                DisplayName = "Sparkin Service",
                Description = "Sparkin指纹解锁服务,提供指纹解锁和蓝牙设备通信功能",
                StartType = ServiceStartMode.Automatic
            };
            
            // 添加安装器
            Installers.Add(serviceProcessInstaller);
            Installers.Add(serviceInstaller);
        }
    }
} 