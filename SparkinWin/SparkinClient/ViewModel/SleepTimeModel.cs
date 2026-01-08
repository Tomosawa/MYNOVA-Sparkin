using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
namespace SparkinClient.ViewModel
{
    public class SleepTimeModel : ViewModelBase
    {
        private ObservableCollection<SleepTimeItem> _items;
        public ObservableCollection<SleepTimeItem> Items
        {
            get => _items;
            set => SetProperty(ref _items, value);
        }

        //private int _selectedId;
        //public int SelectedId
        //{
        //    get => _selectedId;
        //    set => SetProperty(ref _selectedId, value);
        //}

        public SleepTimeModel()
        {
            // 初始化数据
            Items = new ObservableCollection<SleepTimeItem>
            {
                new SleepTimeItem { Value = 5, Name = "5秒" },
                new SleepTimeItem { Value = 10, Name = "10秒" },
                new SleepTimeItem { Value = 15, Name = "15秒" },
                new SleepTimeItem { Value = 30, Name = "30秒" },
                new SleepTimeItem { Value = 60, Name = "1分钟" },
                new SleepTimeItem { Value = 120, Name = "2分钟" },
                new SleepTimeItem { Value = 180, Name = "3分钟" },
                new SleepTimeItem { Value = 300, Name = "5分钟" },
                new SleepTimeItem { Value = 600, Name = "10分钟" },
                new SleepTimeItem { Value = 900, Name = "15分钟" },
                new SleepTimeItem { Value = 1200, Name = "20分钟" },
                new SleepTimeItem { Value = 1500, Name = "25分钟" },
                new SleepTimeItem { Value = 1800, Name = "30分钟" },
                new SleepTimeItem { Value = 2700, Name = "45分钟" },
                new SleepTimeItem { Value = 3600, Name = "1小时" },
                new SleepTimeItem { Value = 7200, Name = "2小时" },
                new SleepTimeItem { Value = 10800, Name = "3小时" },
                new SleepTimeItem { Value = 14400, Name = "4小时" },
                new SleepTimeItem { Value = 18000, Name = "5小时" },
                new SleepTimeItem { Value = 28800, Name = "8小时" },
                new SleepTimeItem { Value = 36000, Name = "10小时" },
                new SleepTimeItem { Value = 86400, Name = "24小时" },
                new SleepTimeItem { Value = 0, Name = "从不" },
            };
        }
    }
}
