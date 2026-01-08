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
namespace SparkinLib.Bluetooth
{
    public class BluetoothCharacteristicInfo
    {
        public Guid Uuid { get; set; }
        public bool SupportsNotifications { get; set; }
        public bool SupportsReading { get; set; }
        public bool SupportsWriting { get; set; }
    }
} 