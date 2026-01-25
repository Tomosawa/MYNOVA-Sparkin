using NLog;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.AccessControl;
using System.Security.Principal;
using System.Text;
using System.Threading.Tasks;
using System.Xml.Linq;
using System.Xml.Serialization;
/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
namespace SparkinLib
{
    public class ConfigManager
    {
        private const string ConfigFileName = "config.xml";
        private static string FolderDataPath = Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData) + "\\Sparkin";
       
        public static ConfigFile LoadConfigFile(Logger log)
        {
            try
            {
                string configPath = Path.Combine(FolderDataPath, ConfigFileName);

                if (File.Exists(configPath))
                {
                    ConfigFile configFile = LoadFromXmlFile(configPath, log);
                    if (configFile != null)
                    {
                        string passwordMask = "";
                        for (int i = 0; i < configFile.LoginPassword.Length; i++)
                            passwordMask += "*";
                        log.Info($"[CONFIG_LOAD]已从配置文件加载设备信息:  LoginUserName={configFile.LoginUserName}, LoginPassword={passwordMask}");
                        return configFile;
                    }
                }
                else
                {
                    log.Info("[CONFIG_LOAD]找不到配置文件，将使用默认设置");
                }
            }
            catch (Exception ex)
            {
                log.Info($"[CONFIG_LOAD]加载配置文件时发生错误: {ex.Message}");
            }
            return new ConfigFile();
        }

        public static bool SaveConfig(ConfigFile configFile, Logger log)
        {
            try
            {
                if (!Directory.Exists(FolderDataPath))
                {
                    Directory.CreateDirectory(FolderDataPath);
                    log.Info($"[CONFIG_SAVE]创建配置文件夹: {FolderDataPath}");
                }
                string configPath = Path.Combine(FolderDataPath, ConfigFileName);

                SaveToXmlFile(configPath, configFile, log);
                log.Info($"[CONFIG_SAVE]已经保存到路径：{configPath}");
                log.Info($"[CONFIG_SAVE]已保存配置:  LoginUserName={configFile.LoginUserName}, LoginPassword={configFile.LoginPassword}");

                // 设置文件权限
                SetFilePermissions(configPath, log);
                return true;
            }
            catch (Exception ex)
            {
                log.Info($"[CONFIG_SAVE]保存配置文件时发生错误: {ex.Message}");
            }
            return false;
        }

        private static void SaveToXmlFile(string filePath, ConfigFile configFile, Logger log)
        {
            try
            {
                var xmlSerializer = new XmlSerializer(typeof(ConfigFile));
                using (var fs = new FileStream(filePath, FileMode.Create, FileAccess.Write, FileShare.Read))
                using (var writer = new StreamWriter(fs))
                {
                    xmlSerializer.Serialize(writer, configFile);
                }
            }
            catch (Exception ex)
            {
                log.Info($"保存配置文件失败: {ex.Message}");
            }
        }

        private static ConfigFile LoadFromXmlFile(string filePath, Logger log)
        {
            try
            {
                var xmlSerializer = new XmlSerializer(typeof(ConfigFile));
                using (var fs = new FileStream(filePath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
                using (var reader = new StreamReader(fs))
                {
                    return (ConfigFile)xmlSerializer.Deserialize(reader);
                }
            }
            catch (Exception ex)
            {
                log.Info($"读取配置文件失败: {ex.Message}");
            }
            return null;
        }

        public static void SetFilePermissions(string filePath, Logger log)
        {
            try
            {
                FileInfo fileInfo = new FileInfo(filePath);
                FileSecurity fileSecurity = fileInfo.GetAccessControl();

                // 检查是否已存在Everyone的权限
                var accessRules = fileSecurity.GetAccessRules(true, true, typeof(NTAccount));
                bool hasEveryoneFullControl = false;

                foreach (FileSystemAccessRule rule in accessRules)
                {
                    if (rule.IdentityReference.Value.Equals("Everyone", StringComparison.OrdinalIgnoreCase) &&
                        rule.FileSystemRights == FileSystemRights.FullControl &&
                        rule.AccessControlType == AccessControlType.Allow)
                    {
                        hasEveryoneFullControl = true;
                        log.Info("已存在文件访问权限，无需设置");
                        break;
                    }
                }

                // 只有不存在时才添加
                if (!hasEveryoneFullControl)
                {
                    fileSecurity.AddAccessRule(
                        new FileSystemAccessRule(
                            "Everyone",
                            FileSystemRights.FullControl,
                            AccessControlType.Allow));

                    fileInfo.SetAccessControl(fileSecurity);
                    log.Info("文件权限修改成功");
                }
            }
            catch (Exception ex)
            {
                log.Info($"设置文件权限失败: {ex.Message}");
            }
        }
    }
}
