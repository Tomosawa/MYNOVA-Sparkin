using NLog;
using SparkinLib;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
namespace SparkinClient
{
    /// <summary>
    /// UpdateWindow.xaml 的交互逻辑
    /// </summary>
    public partial class UpdateWindow 
    {
        /// <summary>
        /// 指纹名称属性
        /// </summary>
        public UpdateInfo updateInfo { get; private set; }
        public string downloadPath { get; set; }

        private UpdateChecker clientUpdater = new UpdateChecker(UpdateChecker.UpdateType.Software);
        private Logger log = LogUtil.GetLogger();
        /// <summary>
        /// 构造函数
        /// </summary>
        /// <param name="fingerName">原始指纹名称</param>
        public UpdateWindow(UpdateInfo updateInfo)
        {
            InitializeComponent();
            this.updateInfo = updateInfo;

            clientUpdater.DownloadProgress += ClientUpdater_DownloadProgress;
            clientUpdater.DownloadCompleted += ClientUpdater_DownloadCompleted;
            clientUpdater.DownloadError += ClientUpdater_DownloadError;
        }

        private void MicaWindow_ContentRendered(object sender, EventArgs e)
        {
            log.Info($"启动开始下载软件更新包");
            Task.Run(async () => await clientUpdater.DownloadUpdateAsync(updateInfo));
        }

        private void ClientUpdater_DownloadCompleted(object sender, string e)
        {
            log.Info($"软件更新包下载完成");
            downloadPath = e;
            Dispatcher.Invoke(() =>
            {
                this.DialogResult = true;
                this.Close();
            });
        }

        private void ClientUpdater_DownloadProgress(object sender, System.ComponentModel.ProgressChangedEventArgs e)
        {
            Dispatcher.Invoke(() =>
            {
                progressBar.Value = e.ProgressPercentage;
                progressText.Text = $"{e.ProgressPercentage}%";
            });
        }

        private void ClientUpdater_DownloadError(object sender, Exception e)
        {
            log.Info($"软件更新包下载出错");
            Dispatcher.Invoke(() =>
            {
                MessageBox.Show(e.Message, "更新失败", MessageBoxButton.OK, MessageBoxImage.Error);
                this.DialogResult = false;
                this.Close();
            });
        }

    }
}
