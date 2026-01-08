using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;
/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
namespace SparkinClient
{
    /// <summary>
    /// RenameWindow.xaml 的交互逻辑
    /// </summary>
    public partial class RenameWindow 
    {
        /// <summary>
        /// 指纹名称属性
        /// </summary>
        public string FingerName { get; private set; }
        
        /// <summary>
        /// 构造函数
        /// </summary>
        /// <param name="fingerName">原始指纹名称</param>
        public RenameWindow(string fingerName)
        {
            InitializeComponent();
            FingerName = fingerName;
            txtFingerName.Text = FingerName;
        }

        /// <summary>
        /// 确认按钮点击事件
        /// </summary>
        private void btnOK_Click(object sender, RoutedEventArgs e)
        {
            FingerName = txtFingerName.Text;
            DialogResult = true;
            Close();
        }

        /// <summary>
        /// 取消按钮点击事件
        /// </summary>
        private void btnCancel_Click(object sender, RoutedEventArgs e)
        {
            DialogResult = false;
            Close();
        }
    }
}
