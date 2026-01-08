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
    public class ConfigFile
    {
        public ConfigFile()
        {
            LoginUserName = string.Empty;
            LoginPassword = string.Empty;
        } 
        public string LoginUserName;
        public string LoginPassword;

    }
}
