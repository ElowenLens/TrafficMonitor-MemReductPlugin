/*************************************************************************
* MemReductPlugin - TrafficMonitor 内存整理插件
* dllmain.h - DLL 实例句柄
*************************************************************************/
#pragma once

//获取本插件 DLL 的实例句柄（在 dllmain.cpp 中赋值）
//用于加载 DLL 内的图标、对话框等资源
extern "C" HINSTANCE GetPluginHInstance();
