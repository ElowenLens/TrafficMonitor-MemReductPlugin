/*************************************************************************
* MemReductPlugin - TrafficMonitor 内存整理插件
* OptionsDialog.cpp - 中文选项设置对话框（纯 Win32 实现）
*************************************************************************/
#include "OptionsDialog.h"
#include "PluginMain.h"
#include "MemCleaner.h"
#include "dllmain.h"
#include "resource.h"
#include <cstdio>

namespace
{
    //对话框打开时的初始配置快照，用于判断用户是否修改了选项
    struct ConfigSnapshot
    {
        unsigned mask;
        int result_mode;
        int hold_seconds;
        bool auto_enable;
        int threshold;
        int interval;
    };

    ConfigSnapshot g_snapshot{};

    void SetCheck(HWND hDlg, int id, bool checked)
    {
        ::SendDlgItemMessageW(hDlg, id, BM_SETCHECK,
            checked ? BST_CHECKED : BST_UNCHECKED, 0);
    }

    bool GetCheck(HWND hDlg, int id)
    {
        return ::SendDlgItemMessageW(hDlg, id, BM_GETCHECK, 0, 0) == BST_CHECKED;
    }

    int GetEditInt(HWND hDlg, int id, int def_value, int min_v, int max_v)
    {
        wchar_t buff[16]{};
        ::GetDlgItemTextW(hDlg, id, buff, 16);
        int value = _wtoi(buff);
        if (value == 0)
            value = def_value;
        if (value < min_v) value = min_v;
        if (value > max_v) value = max_v;
        return value;
    }

    void UpdateControlState(HWND hDlg)
    {
        BOOL enable = GetCheck(hDlg, IDC_CHK_AUTO_ENABLE) ? TRUE : FALSE;
        ::EnableWindow(::GetDlgItem(hDlg, IDC_EDIT_THRESHOLD), enable);
        ::EnableWindow(::GetDlgItem(hDlg, IDC_EDIT_INTERVAL), enable);
        ::EnableWindow(::GetDlgItem(hDlg, IDC_STATIC_THRESHOLD), enable);
        ::EnableWindow(::GetDlgItem(hDlg, IDC_STATIC_INTERVAL), enable);
    }

    //立即整理（测试）按钮
    void OnCleanNow(HWND hDlg)
    {
        unsigned mask = 0;
        if (GetCheck(hDlg, IDC_CHK_SYSTEM_WS))      mask |= MemCleaner::CLEAN_SYSTEM_WORKING_SET;
        if (GetCheck(hDlg, IDC_CHK_PROCESS_WS))     mask |= MemCleaner::CLEAN_PROCESS_WORKING_SET;
        if (GetCheck(hDlg, IDC_CHK_STANDBY))        mask |= MemCleaner::CLEAN_STANDBY_LIST;
        if (GetCheck(hDlg, IDC_CHK_STANDBY_LOW))    mask |= MemCleaner::CLEAN_STANDBY_LOW_PRIORITY;
        if (GetCheck(hDlg, IDC_CHK_FILECACHE))      mask |= MemCleaner::CLEAN_FILE_CACHE;

        if (mask == 0)
        {
            ::MessageBoxW(hDlg, L"请至少勾选一个要释放的项目。", L"内存整理",
                MB_OK | MB_ICONINFORMATION);
            return;
        }

        MemCleaner::CleanResult result = MemCleaner::Clean(mask);
        wchar_t freed_str[64];
        wchar_t msg[512];
        CPluginMain::FormatBytes(result.freed, freed_str, 64);
        if (result.success)
        {
            swprintf_s(msg,
                L"内存整理完成！\r\n\r\n整理前占用：%u%%（可用 %.2f GB）\r\n"
                L"整理后占用：%u%%（可用 %.2f GB）\r\n本次释放：%.2f GB",
                result.mem_before, result.avail_before / 1073741824.0,
                result.mem_after, result.avail_after / 1073741824.0,
                result.freed / 1073741824.0);
            ::MessageBoxW(hDlg, msg, L"内存整理", MB_OK | MB_ICONINFORMATION);
        }
        else
        {
            swprintf_s(msg,
                L"内存整理失败（错误代码 0x%08X）。\r\n\r\n"
                L"常见原因：TrafficMonitor 未以管理员身份运行。\r\n"
                L"请右键 TrafficMonitor 图标，选择“以管理员身份运行”。",
                result.fail_status);
            ::MessageBoxW(hDlg, msg, L"内存整理", MB_OK | MB_ICONWARNING);
        }
        UNREFERENCED_PARAMETER(freed_str);
    }

    INT_PTR CALLBACK OptionsDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
    {
        UNREFERENCED_PARAMETER(lParam);
        switch (message)
        {
        case WM_INITDIALOG:
        {
            CPluginMain& plugin = CPluginMain::Instance();
            wchar_t buff[16];

            //释放项目
            unsigned mask = plugin.GetCleanMask();
            SetCheck(hDlg, IDC_CHK_SYSTEM_WS,  (mask & MemCleaner::CLEAN_SYSTEM_WORKING_SET) != 0);
            SetCheck(hDlg, IDC_CHK_PROCESS_WS, (mask & MemCleaner::CLEAN_PROCESS_WORKING_SET) != 0);
            SetCheck(hDlg, IDC_CHK_STANDBY,    (mask & MemCleaner::CLEAN_STANDBY_LIST) != 0);
            SetCheck(hDlg, IDC_CHK_STANDBY_LOW,(mask & MemCleaner::CLEAN_STANDBY_LOW_PRIORITY) != 0);
            SetCheck(hDlg, IDC_CHK_FILECACHE,  (mask & MemCleaner::CLEAN_FILE_CACHE) != 0);

            //结果显示方式
            CPluginMain::ResultMode mode = plugin.GetResultMode();
            SetCheck(hDlg, IDC_RADIO_RESULT_ITEM,   mode == CPluginMain::RM_ITEM);
            SetCheck(hDlg, IDC_RADIO_RESULT_NOTIFY, mode == CPluginMain::RM_NOTIFY);
            SetCheck(hDlg, IDC_RADIO_RESULT_NONE,   mode == CPluginMain::RM_NONE);
            swprintf_s(buff, L"%d", plugin.GetResultHoldSeconds());
            ::SetDlgItemTextW(hDlg, IDC_EDIT_HOLD, buff);

            //自动整理
            SetCheck(hDlg, IDC_CHK_AUTO_ENABLE, plugin.IsAutoCleanEnabled());
            swprintf_s(buff, L"%d", plugin.GetAutoThreshold());
            ::SetDlgItemTextW(hDlg, IDC_EDIT_THRESHOLD, buff);
            swprintf_s(buff, L"%d", plugin.GetAutoInterval());
            ::SetDlgItemTextW(hDlg, IDC_EDIT_INTERVAL, buff);

            UpdateControlState(hDlg);
            return (INT_PTR)TRUE;
        }

        case WM_COMMAND:
        {
            int id = LOWORD(wParam);
            switch (id)
            {
            case IDC_CHK_AUTO_ENABLE:       //启用/停用自动整理时刷新控件状态
                UpdateControlState(hDlg);
                return (INT_PTR)TRUE;
            case IDC_BTN_CLEAN_NOW:         //立即整理（测试）
                OnCleanNow(hDlg);
                return (INT_PTR)TRUE;
            case IDOK:
            {
                CPluginMain& plugin = CPluginMain::Instance();

                unsigned mask = 0;
                if (GetCheck(hDlg, IDC_CHK_SYSTEM_WS))      mask |= MemCleaner::CLEAN_SYSTEM_WORKING_SET;
                if (GetCheck(hDlg, IDC_CHK_PROCESS_WS))     mask |= MemCleaner::CLEAN_PROCESS_WORKING_SET;
                if (GetCheck(hDlg, IDC_CHK_STANDBY))        mask |= MemCleaner::CLEAN_STANDBY_LIST;
                if (GetCheck(hDlg, IDC_CHK_STANDBY_LOW))    mask |= MemCleaner::CLEAN_STANDBY_LOW_PRIORITY;
                if (GetCheck(hDlg, IDC_CHK_FILECACHE))      mask |= MemCleaner::CLEAN_FILE_CACHE;

                CPluginMain::ResultMode mode = CPluginMain::RM_ITEM;
                if (GetCheck(hDlg, IDC_RADIO_RESULT_NOTIFY))    mode = CPluginMain::RM_NOTIFY;
                else if (GetCheck(hDlg, IDC_RADIO_RESULT_NONE)) mode = CPluginMain::RM_NONE;

                plugin.SetCleanMask(mask);
                plugin.SetResultMode(mode);
                plugin.SetResultHoldSeconds(GetEditInt(hDlg, IDC_EDIT_HOLD, 5, 3, 600));
                plugin.SetAutoCleanEnabled(GetCheck(hDlg, IDC_CHK_AUTO_ENABLE));
                plugin.SetAutoThreshold(GetEditInt(hDlg, IDC_EDIT_THRESHOLD, 85, 1, 100));
                plugin.SetAutoInterval(GetEditInt(hDlg, IDC_EDIT_INTERVAL, 30, 1, 1440));
                plugin.SaveConfig();

                ::EndDialog(hDlg, IDOK);
                return (INT_PTR)TRUE;
            }
            case IDCANCEL:
                ::EndDialog(hDlg, IDCANCEL);
                return (INT_PTR)TRUE;
            default:
                break;
            }
            break;
        }
        default:
            break;
        }
        return (INT_PTR)FALSE;
    }
}

bool ShowMemReductOptionsDialog(HWND hParent)
{
    CPluginMain& plugin = CPluginMain::Instance();

    //记录初始配置，用于判断是否有修改
    g_snapshot.mask = plugin.GetCleanMask();
    g_snapshot.result_mode = (int)plugin.GetResultMode();
    g_snapshot.hold_seconds = plugin.GetResultHoldSeconds();
    g_snapshot.auto_enable = plugin.IsAutoCleanEnabled();
    g_snapshot.threshold = plugin.GetAutoThreshold();
    g_snapshot.interval = plugin.GetAutoInterval();

    INT_PTR ret = ::DialogBoxParamW(GetPluginHInstance(),
        MAKEINTRESOURCEW(IDD_OPTIONS), hParent, OptionsDlgProc, 0);

    if (ret != IDOK)
        return false;

    return plugin.GetCleanMask() != g_snapshot.mask
        || (int)plugin.GetResultMode() != g_snapshot.result_mode
        || plugin.GetResultHoldSeconds() != g_snapshot.hold_seconds
        || plugin.IsAutoCleanEnabled() != g_snapshot.auto_enable
        || plugin.GetAutoThreshold() != g_snapshot.threshold
        || plugin.GetAutoInterval() != g_snapshot.interval;
}
