/*************************************************************************
* MemReductPlugin - TrafficMonitor 内存整理插件
* MemCleaner.h - 基于 NT 原生接口的内存清理模块
*
* 实现思路参考 henrypp/memreduct：
*   通过 NtSetSystemInformation 发出系统内存列表清理命令。
*   命令常量取自 memreduct 所使用的 routine 库 ntapi.h，
*   并已在本机（Windows 11, 管理员权限）实测验证生效。
*************************************************************************/
#pragma once
#include <windows.h>

namespace MemCleaner
{
    //可清理的项目（位掩码）
    enum CleanItems
    {
        CLEAN_SYSTEM_WORKING_SET    = 1 << 0,   //系统工作集
        CLEAN_PROCESS_WORKING_SET   = 1 << 1,   //所有进程工作集
        CLEAN_STANDBY_LIST          = 1 << 2,   //备用列表
        CLEAN_STANDBY_LOW_PRIORITY  = 1 << 3,   //低优先级备用页（优先级0）
        CLEAN_FILE_CACHE            = 1 << 4,   //系统文件缓存
    };

    //一次清理的结果
    struct CleanResult
    {
        bool success;               //所有勾选项目均执行成功
        bool privilege_failed;      //是否因权限不足而失败（需以管理员身份运行）
        DWORD fail_status;          //最后一个失败的 NTSTATUS 代码
        DWORD mem_before;           //整理前物理内存占用（百分比）
        DWORD mem_after;            //整理后物理内存占用（百分比）
        unsigned long long avail_before;    //整理前可用物理内存（字节）
        unsigned long long avail_after;     //整理后可用物理内存（字节）
        unsigned long long freed;           //释放量 = 整理后可用 - 整理前可用（字节）
        unsigned mask_used;                 //实际执行的项目

        CleanResult()
            : success(false), privilege_failed(false), fail_status(0)
            , mem_before(0), mem_after(0)
            , avail_before(0), avail_after(0), freed(0), mask_used(0)
        {}
    };

    //当前进程是否以管理员（提升）权限运行
    bool IsElevated();

    //启用所需的特权（SeProfileSingleProcessPrivilege、SeIncreaseQuotaPrivilege）
    bool EnableRequiredPrivileges();

    //执行一次内存清理。mask 为 CleanItems 的组合，返回整理前后的内存信息
    CleanResult Clean(unsigned mask);
}
