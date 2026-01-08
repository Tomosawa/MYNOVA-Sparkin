using System;
using System.Xml.Serialization;
/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
// 定义更新信息类
[XmlRoot("UpdateInfo")]
[Serializable]
public class UpdateInfo
{
    [XmlElement("Version")]
    public string Version { get; set; }

    [XmlElement("FileName")]
    public string FileName { get; set; }

    [XmlElement("HashCRC32")]
    public string HashCRC32 { get; set; }

    [XmlElement("Description")]
    public string Description { get; set; }
}