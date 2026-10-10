/*************************************************************************
* MemReductPlugin - TrafficMonitor 内存整理插件
* MemCleaner.cpp - 基于 NT 原生接口的内存清理模块实现
*************************************************************************/
#include "MemCleaner.h"
#include <cstdio>

//=========================================================================
// NT 原生接口定义（动态加载，兼容 x86/x64）
//=========================================================================
namespace
{
    //系统信息类别（与 routine/ntapi.h 及 winsiderss/phnt 定义一致）
    const ULONG SystemMemoryListInformation   = 80;   //内存列表（备用列表/工作集）
    const ULONG SystemFileCacheInformationEx  = 81;   //系统文件缓存（设置用）
    const ULONG SystemFileCacheInformation    = 21;   //系统文件缓存（查询用）

    //SYSTEM_MEMORY_LIST_COMMAND 枚举值
    //（与 memreduct 的 routine 库、当前 winsiderss/phnt 官方枚举一致，
    //  并已在本机实测验证：命令2/4/5均返回STATUS_SUCCESS且内存明显变化）
    const ULONG MemoryEmptyWorkingSets          = 2;    //清空系统与所有进程的工作集
    const ULONG MemoryPurgeStandbyList          = 4;    //清空备用列表
    const ULONG MemoryPurgeLowPriorityStandbyList = 5;  //清空低优先级（0级）备用页

    //特权索引（RtlAdjustPrivilege 使用）
    const ULONG SE_INCREASE_QUOTA_PRIVILEGE           = 5;
    const ULONG SE_PROFILE_SINGLE_PROCESS_PRIVILEGE   = 13;

    typedef LONG(NTAPI* t_NtSetSystemInformation)(ULONG SystemInformationClass,
        PVOID SystemInformation, ULONG SystemInformationLength);
    typedef LONG(NTAPI* t_NtQuerySystemInformation)(ULONG SystemInformationClass,
        PVOID SystemInformation, ULONG SystemInformationLength, PULONG ReturnLength);
    typedef LONG(NTAPI* t_RtlAdjustPrivilege)(ULONG Privilege, BOOLEAN Enable,
        BOOLEAN CurrentThread, PBOOLEAN PreviousValue);

    t_NtSetSystemInformation     pfnNtSetSystemInformation = nullptr;
    t_NtQuerySystemInformation   pfnNtQuerySystemInformation = nullptr;
    t_RtlAdjustPrivilege         pfnRtlAdjustPrivilege = nullptr;

    //SYSTEM_FILECACHE_INFORMATION 结构（见 routine/ntapi.h）
    struct SYSTEM_FILECACHE_INFORMATION
    {
        ULONG_PTR CurrentSize;
        ULONG_PTR PeakSize;
        ULONG     PageFaultCount;
        ULONG_PTR MinimumWorkingSet;
        ULONG_PTR MaximumWorkingSet;
        ULONG_PTR CurrentSizeIncludingTransitionInPages;
        ULONG_PTR PeakSizeIncludingTransitionInPages;
        ULONG     TransitionRePurposeCount;
        ULONG     Flags;
    };

    //物理内存信息（GlobalMemoryStatusEx 的封装）
    struct MemoryStatus
    {
        DWORD percent;                  //物理内存占用百分比
        unsigned long long avail;       //可用物理内存（字节）
        unsigned long long total;       //物理内存总量（字节）
    };

    bool InitNtdll()
    {
        if (pfnNtSetSystemInformation != nullptr)
            return true;
        HMODULE hNtdll = ::GetModuleHandleW(L"ntdll.dll");
        if (hNtdll == nullptr)
            return false;
        pfnNtSetSystemInformation = (t_NtSetSystemInformation)::GetProcAddress(hNtdll, "NtSetSystemInformation");
        pfnNtQuerySystemInformation = (t_NtQuerySystemInformation)::GetProcAddress(hNtdll, "NtQuerySystemInformation");
        pfnRtlAdjustPrivilege = (t_RtlAdjustPrivilege)::GetProcAddress(hNtdll, "RtlAdjustPrivilege");
        return pfnNtSetSystemInformation != nullptr && pfnNtQuerySystemInformation != nullptr
            && pfnRtlAdjustPrivilege != nullptr;
    }

    bool QueryMemoryStatus(MemoryStatus& status)
    {
        MEMORYSTATUSEX mse{};
        mse.dwLength = sizeof(mse);
        if (!::GlobalMemoryStatusEx(&mse))
            return false;
        status.percent = mse.dwMemoryLoad;
        status.avail = mse.ullAvailPhys;
        status.total = mse.ullTotalPhys;
        return true;
    }

    const DWORD NT_SUCCESS_V(DWORD status) { return status < 0x40000000; }   //NT_SUCCESS 宏
}

namespace MemCleaner
{
    bool IsElevated()
    {
        BOOL is_admin = FALSE;
        PSID admin_group = nullptr;
        SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
        if (::AllocateAndInitializeSid(&nt_authority, 2,
            SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
            0, 0, 0, 0, 0, 0, &admin_group))
        {
            ::CheckTokenMembership(nullptr, admin_group, &is_admin);
            ::FreeSid(admin_group);
        }
        return is_admin != FALSE;
    }

    bool EnableRequiredPrivileges()
    {
        if (!InitNtdll())
            return false;
        BOOLEAN prev = FALSE;
        //两个特权都启用，失败不立刻返回，分别记录即可（此处只要有一个失败即返回false）
        LONG st1 = pfnRtlAdjustPrivilege(SE_PROFILE_SINGLE_PROCESS_PRIVILEGE, TRUE, FALSE, &prev);
        LONG st2 = pfnRtlAdjustPrivilege(SE_INCREASE_QUOTA_PRIVILEGE, TRUE, FALSE, &prev);
        return NT_SUCCESS_V((DWORD)st1) && NT_SUCCESS_V((DWORD)st2);
    }

    CleanResult Clean(unsigned mask)
    {
        CleanResult result;
        result.mask_used = mask;
        if (mask == 0)
        {
            result.success = true;      //未勾选任何项目，视为无事可做
            return result;
        }
        if (!InitNtdll())
        {
            result.fail_status = (DWORD)0xC0000001;  //STATUS_UNSUCCESSFUL
            return result;
        }

        //启用特权（清理命令需要 SeProfileSingleProcessPrivilege、SeIncreaseQuotaPrivilege）
        if (!EnableRequiredPrivileges())
            result.privilege_failed = true;

        //整理前的内存信息
        MemoryStatus before{};
        QueryMemoryStatus(before);
        result.mem_before = before.percent;
        result.avail_before = before.avail;

        //1) 系统工作集 + 进程工作集
        //   说明：MemoryEmptyWorkingSets 命令会一次性清空系统工作集和所有进程的工作集，
        //   与 memreduct 的做法一致，因此这里两个选项任一勾选即执行该命令。
        if ((mask & (CLEAN_SYSTEM_WORKING_SET | CLEAN_PROCESS_WORKING_SET)) != 0)
        {
            ULONG command = MemoryEmptyWorkingSets;
            DWORD status = (DWORD)pfnNtSetSystemInformation(SystemMemoryListInformation,
                &command, sizeof(command));
            if (!NT_SUCCESS_V(status))
            {
                result.fail_status = status;
            }
        }

        //2) 备用列表
        if ((mask & CLEAN_STANDBY_LIST) != 0)
        {
            ULONG command = MemoryPurgeStandbyList;
            DWORD status = (DWORD)pfnNtSetSystemInformation(SystemMemoryListInformation,
                &command, sizeof(command));
            if (!NT_SUCCESS_V(status))
                result.fail_status = status;
        }

        //3) 低优先级备用页（优先级0备用列表）
        if ((mask & CLEAN_STANDBY_LOW_PRIORITY) != 0)
        {
            ULONG command = MemoryPurgeLowPriorityStandbyList;
            DWORD status = (DWORD)pfnNtSetSystemInformation(SystemMemoryListInformation,
                &command, sizeof(command));
            if (!NT_SUCCESS_V(status))
                result.fail_status = status;
        }

        //4) 系统文件缓存
        //   将最小/最大工作集设为 MAXSIZE_T 触发缓存收缩（与 memreduct 一致）
        if ((mask & CLEAN_FILE_CACHE) != 0)
        {
            SYSTEM_FILECACHE_INFORMATION sfci{};
            sfci.MinimumWorkingSet = (ULONG_PTR)-1;
            sfci.MaximumWorkingSet = (ULONG_PTR)-1;
            DWORD status = (DWORD)pfnNtSetSystemInformation(SystemFileCacheInformationEx,
                &sfci, sizeof(sfci));
            if (!NT_SUCCESS_V(status))
                result.fail_status = status;
        }

        //整理后的内存信息，计算释放量
        MemoryStatus after{};
        QueryMemoryStatus(after);
        result.mem_after = after.percent;
        result.avail_after = after.avail;
        result.freed = (after.avail > before.avail) ? (after.avail - before.avail) : 0;
        result.success = NT_SUCCESS_V(result.fail_status);

        return result;
    }
}
