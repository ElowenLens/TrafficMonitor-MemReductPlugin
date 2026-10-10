/*************************************************************************
* MemReductPlugin - TrafficMonitor 内存整理插件
* OptionsDialog.h - 中文选项设置对话框（纯 Win32 实现）
*************************************************************************/
#pragma once
#include <windows.h>

//显示选项设置对话框，返回 true 表示用户修改了选项（点击了“确定”且内容有变化）
bool ShowMemReductOptionsDialog(HWND hParent);
