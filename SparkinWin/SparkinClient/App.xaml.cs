using System;
using System.Collections.Generic;
using System.Configuration;
using System.Data;
using System.Linq;
using System.Threading.Tasks;
using System.Windows;
using System.Threading;
using System.Diagnostics;
using System.Globalization;
using System.IO.Pipes;
using System.IO;
using System.Reflection;
using SparkinLib;
/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
namespace SparkinClient
{
    /// <summary>
    /// App.xaml 的交互逻辑
    /// </summary>
    public partial class App : Application
    {
        private static Mutex _mutex = null;

        protected override void OnStartup(StartupEventArgs e)
        {
            base.OnStartup(e);
            LoadLanguage();
            // 创建互斥体，确保只有一个程序实例运行
            const string appName = "SparkinClientApp";
            bool createNew;
            
            _mutex = new Mutex(true, appName, out createNew);
            
            if (!createNew)
            {
                // 如果应用程序已经在运行，则唤醒已有窗口
                SendShowCommandToPrimaryInstance();
                Shutdown();
                return;
            }
        }

        private void SendShowCommandToPrimaryInstance()
        {
            try
            {
                // 连接到主实例的管道并发送命令
                using (var client = new NamedPipeClientStream(".", "Sparkin-ShowWindow", PipeDirection.Out))
                {
                    client.Connect(2000); // 超时时间2秒

                    using (var writer = new StreamWriter(client))
                    {
                        writer.WriteLine("ShowWindow");
                        writer.Flush();
                    }
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show("无法激活已运行的应用程序实例：" + ex.Message);
            }
        }

        protected override void OnExit(ExitEventArgs e)
        {
            if (_mutex != null && _mutex.WaitOne(0, false))
            {
                _mutex.ReleaseMutex();
            }
            base.OnExit(e);
        }


        public void LoadLanguage()
        {
            // 获取当前线程的用户界面文化
            CultureInfo currentUICulture = CultureInfo.CurrentUICulture;
            Debug.WriteLine("Current UI Culture: " + currentUICulture.Name);

            List<ResourceDictionary> dictionaryList = new List<ResourceDictionary>();
            foreach (ResourceDictionary dictionary in Application.Current.Resources.MergedDictionaries)
            {
                dictionaryList.Add(dictionary);
            }
            string requestedCulture = string.Format(@"Languages\StringResource.{0}.xaml", currentUICulture.Name);
            ResourceDictionary resourceDictionary = dictionaryList.FirstOrDefault(d => d.Source.OriginalString.Equals(requestedCulture));
            if (resourceDictionary == null)
            {
                requestedCulture = @"Languages\StringResource.xaml";
                resourceDictionary = dictionaryList.FirstOrDefault(d => d.Source.OriginalString.Equals(requestedCulture));
            }
            if (resourceDictionary != null)
            {
                Application.Current.Resources.MergedDictionaries.Remove(resourceDictionary);
                Application.Current.Resources.MergedDictionaries.Add(resourceDictionary);
            }
        }
    }
}
