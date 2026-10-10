/*************************************************************************
* MemReductPlugin - TrafficMonitor 内存整理插件
* dllmain.cpp - DLL 入口与插件导出函数
*************************************************************************/
#include "PluginInterface.h"
#include "PluginMain.h"
#include "dllmain.h"

//=========================================================================
//TrafficMonitor 插件规范要求的唯一导出函数：
//返回插件接口对象的指针。对象为静态存在，由主程序使用，不负责释放。
//=========================================================================
#ifdef __cplusplus
extern "C" {
#endif

__declspec(dllexport) ITMPlugin* TMPluginGetInstance();

#ifdef __cplusplus
}
#endif

ITMPlugin* TMPluginGetInstance()
{
    return &CPluginMain::Instance();
}

//=========================================================================
//DLL 入口：保存实例句柄（用于加载图标和对话框资源）
//=========================================================================
static HINSTANCE g_hInstance = nullptr;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
    UNREFERENCED_PARAMETER(lpReserved);
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        g_hInstance = hModule;
        DisableThreadLibraryCalls(hModule);
        break;
    case DLL_PROCESS_DETACH:
        break;
    default:
        break;
    }
    return TRUE;
}

//供其他编译单元获取 DLL 实例句柄
HINSTANCE GetPluginHInstance()
{
    return g_hInstance;
}
