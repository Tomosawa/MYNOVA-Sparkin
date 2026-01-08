using NLog;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;
/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
namespace SparkinLib
{
    public class LogUtil
    {
        public static Logger GetLogger(string name = "")
        {
            if(string.IsNullOrEmpty(name))
            {
                name = Assembly.GetEntryAssembly().GetName().Name;
            }
            return LogManager.GetLogger(name);
        }
    }
}
