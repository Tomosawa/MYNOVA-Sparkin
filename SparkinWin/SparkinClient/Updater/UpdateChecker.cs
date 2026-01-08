using System;
using System.IO;
using System.Net.Http;
using System.Threading.Tasks;
using System.Xml.Serialization;
using System.ComponentModel;
using NLog;
using SparkinLib;
/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
public class UpdateChecker
{
    // 下载进度事件参数
    public class DownloadProgressEventArgs : EventArgs
    {
        public int ProgressPercentage { get; set; }
        public string StatusMessage { get; set; }
    }

    public enum UpdateType
    {
        Firmware,
        Software
    }
    // 事件定义
    public event ProgressChangedEventHandler DownloadProgress;
    public event EventHandler<UpdateInfo> UpdateAvailable;
    public event EventHandler<UpdateInfo> UpdateUnavailable;
    public event EventHandler<string> DownloadCompleted;
    public event EventHandler<Exception> DownloadError;

    private readonly string _baseUrl = ""; //Config Update Server URL like https://xxx.com/sparkin/
    private readonly string firmwareXmlPath = "/FirmwareUpdate.xml";
    private readonly string winClientXmlPath = "/WinClientUpdate.xml";
    private readonly string _downloadDirectory;
    private string _updateXmlPath;
    // 日志记录
    private Logger log = LogUtil.GetLogger();

    public UpdateChecker(UpdateType updateType)
    {
        // 初始化下载目录
        _downloadDirectory = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData),
            "Sparkin\\Update");
        if(!Directory.Exists(_downloadDirectory))
            Directory.CreateDirectory(_downloadDirectory);

        switch (updateType)
        {
            case UpdateType.Firmware:
                _updateXmlPath = firmwareXmlPath;
                break;
            case UpdateType.Software:
                _updateXmlPath = winClientXmlPath;
                break;
        }
    }

    // 检查更新
    public async Task CheckForUpdateAsync(string currentVersion)
    {
        try
        {
            // 如果没配置地址则不检查
            if (string.IsNullOrEmpty(_baseUrl))
                return;
            using (var client = new HttpClient())
            {
                var xmlContent = await client.GetStringAsync(_baseUrl + _updateXmlPath);
                var serializer = new XmlSerializer(typeof(UpdateInfo));
                using (var reader = new StringReader(xmlContent))
                {
                    var updateInfo = (UpdateInfo)serializer.Deserialize(reader);
                    Version localVersion = new Version(currentVersion);
                    Version remoteVersion = new Version(updateInfo.Version);
                    if(remoteVersion > localVersion)
                        OnUpdateAvailable(updateInfo);
                    else
                        OnUpdateUnavailable(updateInfo);
                }
            }
        }
        catch (Exception ex)
        {
            // 异常处理逻辑
            log.Error($"检查更新失败: {ex.Message}");
        }
    }

    // 下载更新
    public async Task DownloadUpdateAsync(UpdateInfo updateInfo)
    {
        try
        {
            var downloadUrl = new Uri(new Uri(_baseUrl), updateInfo.FileName);
            var filePath = Path.Combine(_downloadDirectory, updateInfo.FileName);

            if (File.Exists(filePath))
            {
               log.Info($"[UPDATE_DOWNLOAD]下载路径已存在同名文件: {filePath}");
               string hash = CRC32Tool.CalculateFileCrc32(filePath);
                if(!string.IsNullOrEmpty(hash) && hash == updateInfo.HashCRC32)
                {
                    log.Info($"[UPDATE_DOWNLOAD]CRC32校验成功，文件无需重新下载！");
                    OnDownloadCompleted(filePath);
                    return;
                }
                else
                {
                    log.Info($"[UPDATE_DOWNLOAD]CRC32校验失败，删除后重新下载！");
                    File.Delete(filePath);
                }
            }

            using (var client = new HttpClient())
            {
                using (var response = await client.GetAsync(downloadUrl, HttpCompletionOption.ResponseHeadersRead))
                {
                    response.EnsureSuccessStatusCode();

                    var totalBytes = response.Content.Headers.ContentLength ?? -1L;
                    var totalBytesRead = 0L;
                    var buffer = new byte[8192];

                    using (var fileStream = new FileStream(filePath, FileMode.Create, FileAccess.Write, FileShare.None))
                    {
                        using (var httpStream = await response.Content.ReadAsStreamAsync())
                        {
                            while (true)
                            {
                                var count = await httpStream.ReadAsync(buffer, 0, buffer.Length);
                                if (count == 0)
                                    break;

                                await fileStream.WriteAsync(buffer, 0, count);
                                totalBytesRead += count;

                                // 进度报告
                                var progress = totalBytes == -1 ? 0 : (int)((totalBytesRead * 100) / totalBytes);
                                OnDownloadProgress(progress, $"下载中... {totalBytesRead / 1024} KB");
                            }
                            fileStream.Close();
                            httpStream.Close();
                            OnDownloadCompleted(filePath);
                        }
                    }
                }
            }
        }
        catch (Exception ex)
        {
            // 下载异常处理
            log.Error($"[UPDATE_DOWNLOAD]下载失败: {ex.Message}");
            OnDownloadFailed(ex);
        }
    }

    // 事件触发方法
    protected virtual void OnDownloadProgress(int percentage, string status)
    {
        DownloadProgress?.Invoke(this, new ProgressChangedEventArgs(percentage, status));
    }

    protected virtual void OnUpdateAvailable(UpdateInfo updateInfo)
    {
        UpdateAvailable?.Invoke(this, updateInfo);
    }

    protected virtual void OnUpdateUnavailable(UpdateInfo updateInfo)
    {
        UpdateUnavailable?.Invoke(this, updateInfo);
    }

    protected virtual void OnDownloadCompleted(string filePath)
    {
        DownloadCompleted?.Invoke(this, filePath);
    }
    protected virtual void OnDownloadFailed(Exception error)
    {
        DownloadError?.Invoke(this, error);
    }
}