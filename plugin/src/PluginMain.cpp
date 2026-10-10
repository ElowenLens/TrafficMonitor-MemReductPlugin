/*************************************************************************
* MemReductPlugin - TrafficMonitor 内存整理插件
* PluginMain.cpp - 插件主类实现
*************************************************************************/
#include "PluginMain.h"
#include "MemCleaner.h"
#include "OptionsDialog.h"
#include "dllmain.h"
#include "resource.h"
#include <cstdio>
#include <cwchar>

//插件唯一实例
static CPluginMain g_plugin;

CPluginMain& CPluginMain::Instance()
{
    return g_plugin;
}

CPluginMain::CPluginMain()
    : m_item(this)
{
}

//=========================================================================
//显示项类
//=========================================================================
const wchar_t* CMemoryItem::GetItemName() const
{
    return L"内存整理";
}

const wchar_t* CMemoryItem::GetItemId() const
{
    return L"MemReductItem";
}

const wchar_t* CMemoryItem::GetItemLableText() const
{
    return L"内存";
}

const wchar_t* CMemoryItem::GetItemValueText() const
{
    return m_plugin->m_value_text.c_str();
}

const wchar_t* CMemoryItem::GetItemValueSampleText() const
{
    return L"释放888.8GB";
}

int CMemoryItem::OnMouseEvent(MouseEventType type, int x, int y, void* hWnd, int flag)
{
    UNREFERENCED_PARAMETER(x);
    UNREFERENCED_PARAMETER(y);
    UNREFERENCED_PARAMETER(hWnd);
    UNREFERENCED_PARAMETER(flag);
    switch (type)
    {
    case IPluginItem::MT_DBCLICKED:
        //双击插件显示项：立即执行一次内存整理，无需确认
        m_plugin->DoClean(false);
        return 1;       //已处理，主程序不再响应
    case IPluginItem::MT_RCLICKED:
        //返回0，由主程序弹出右键菜单（其中包含本插件的“立即整理内存”命令）
        return 0;
    default:
        return 0;
    }
}

//=========================================================================
//ITMPlugin 接口
//=========================================================================
IPluginItem* CPluginMain::GetItem(int index)
{
    //本插件只提供一个显示项目
    if (index == 0)
        return &m_item;
    return nullptr;
}

void CPluginMain::OnInitialize(ITrafficMonitor* pApp)
{
    m_pTrafficMonitor = pApp;
}

void CPluginMain::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
{
    if (index == ITMPlugin::EI_CONFIG_DIR && data != nullptr)
    {
        SetConfigDir(data);
    }
}

void CPluginMain::OnMonitorInfo(const MonitorInfo& monitor_info)
{
    //保存主程序获取到的内存占用百分比（0~100）
    m_mem_percent = monitor_info.memory_usage;
}

const wchar_t* CPluginMain::GetInfo(PluginInfoIndex index)
{
    switch (index)
    {
    case TMI_NAME:
        return L"内存整理";
    case TMI_DESCRIPTION:
        return L"基于 NT 原生接口的一键内存整理插件，可清理系统/进程工作集、备用列表、低优先级备用页与系统文件缓存。双击插件显示项或使用右键菜单命令即可整理内存。参考 memreduct 的实现思路。";
    case TMI_AUTHOR:
        return L"ElowenLens";
    case TMI_COPYRIGHT:
        return L"Copyright (C) 2026";
    case TMI_VERSION:
        return L"1.0.0";
    case TMI_URL:
        return L"https://github.com/henrypp/memreduct";
    default:
        return L"";
    }
}

void CPluginMain::DataRequired()
{
    if (!m_config_loaded)
        LoadConfig();

    //自动整理检查（内部做了节流，每5秒检查一次条件）
    CheckAutoClean();

    //刷新显示文本
    UpdateDisplayText();
}

const wchar_t* CPluginMain::GetTooltipInfo()
{
    return m_tooltip_text.c_str();
}

void* CPluginMain::GetPluginIcon()
{
    return ::LoadIconW(GetPluginHInstance(), MAKEINTRESOURCEW(IDI_PLUGIN));
}

//---------------- 插件命令（显示在右键菜单中） ----------------
int CPluginMain::GetCommandCount()
{
    return 1;
}

const wchar_t* CPluginMain::GetCommandName(int command_index)
{
    if (command_index == 0)
        return L"立即整理内存";
    return nullptr;
}

void* CPluginMain::GetCommandIcon(int command_index)
{
    if (command_index == 0)
        return ::LoadIconW(GetPluginHInstance(), MAKEINTRESOURCEW(IDI_PLUGIN));
    return nullptr;
}

int CPluginMain::IsCommandChecked(int command_index)
{
    UNREFERENCED_PARAMETER(command_index);
    return 0;
}

void CPluginMain::OnPluginCommand(int command_index, void* hWnd, void* para)
{
    UNREFERENCED_PARAMETER(hWnd);
    UNREFERENCED_PARAMETER(para);
    if (command_index == 0)
        DoClean(false);     //右键菜单“立即整理内存”
}

//---------------- 选项设置对话框 ----------------
ITMPlugin::OptionReturn CPluginMain::ShowOptionsDialog(void* hParent)
{
    if (!m_config_loaded)
        LoadConfig();
    //弹出中文设置对话框（纯 Win32 实现）
    if (ShowMemReductOptionsDialog((HWND)hParent))
        return OR_OPTION_CHANGED;
    return OR_OPTION_UNCHANGED;
}

//=========================================================================
//配置
//=========================================================================
void CPluginMain::SetConfigDir(const wchar_t* dir)
{
    m_config_dir = dir;
    //去除末尾的反斜杠后再拼接，避免出现 "dir\\" 与 "dir" 不一致的问题
    if (!m_config_dir.empty() && (m_config_dir.back() == L'\\' || m_config_dir.back() == L'/'))
        m_config_dir.pop_back();
    m_config_file = m_config_dir + L"\\MemReductPlugin.ini";
    LoadConfig();
}

unsigned CPluginMain::GetCleanMask() const
{
    return m_clean_mask;
}

void CPluginMain::SetCleanMask(unsigned mask)
{
    m_clean_mask = mask;
}

void CPluginMain::LoadConfig()
{
    if (m_config_file.empty())
        return;
    const wchar_t* file = m_config_file.c_str();
    m_clean_mask = (unsigned)::GetPrivateProfileIntW(L"config", L"clean_mask", 0x1C, file);
    m_result_mode = (ResultMode)::GetPrivateProfileIntW(L"config", L"result_mode", RM_ITEM, file);
    if (m_result_mode < RM_ITEM || m_result_mode > RM_NONE)
        m_result_mode = RM_ITEM;
    m_result_hold_seconds = ::GetPrivateProfileIntW(L"config", L"result_hold_seconds", 5, file);
    if (m_result_hold_seconds < 3) m_result_hold_seconds = 3;
    if (m_result_hold_seconds > 600) m_result_hold_seconds = 600;
    m_auto_enable = (::GetPrivateProfileIntW(L"config", L"auto_enable", 0, file) != 0);
    m_auto_threshold = ::GetPrivateProfileIntW(L"config", L"auto_threshold", 85, file);
    if (m_auto_threshold < 1) m_auto_threshold = 1;
    if (m_auto_threshold > 100) m_auto_threshold = 100;
    m_auto_interval = ::GetPrivateProfileIntW(L"config", L"auto_interval", 30, file);
    if (m_auto_interval < 1) m_auto_interval = 1;
    m_config_loaded = true;
}

void CPluginMain::SaveConfig()
{
    if (m_config_file.empty())
        return;
    wchar_t buff[32];
    const wchar_t* file = m_config_file.c_str();
    swprintf_s(buff, L"%u", m_clean_mask);
    ::WritePrivateProfileStringW(L"config", L"clean_mask", buff, file);
    swprintf_s(buff, L"%d", (int)m_result_mode);
    ::WritePrivateProfileStringW(L"config", L"result_mode", buff, file);
    swprintf_s(buff, L"%d", m_result_hold_seconds);
    ::WritePrivateProfileStringW(L"config", L"result_hold_seconds", buff, file);
    ::WritePrivateProfileStringW(L"config", L"auto_enable", m_auto_enable ? L"1" : L"0", file);
    swprintf_s(buff, L"%d", m_auto_threshold);
    ::WritePrivateProfileStringW(L"config", L"auto_threshold", buff, file);
    swprintf_s(buff, L"%d", m_auto_interval);
    ::WritePrivateProfileStringW(L"config", L"auto_interval", buff, file);
}

//=========================================================================
//核心功能：执行一次内存整理
//=========================================================================
void CPluginMain::DoClean(bool auto_clean)
{
    if (m_cleaning)     //防止重入
        return;
    m_cleaning = true;

    if (!m_config_loaded)
        LoadConfig();

    MemCleaner::CleanResult result = MemCleaner::Clean(GetCleanMask());

    //记录结果
    m_last_clean_tick = ::GetTickCount64();
    m_last_freed = result.freed;
    m_last_mem_before = result.mem_before;
    m_last_mem_after = result.mem_after;
    m_has_cleaned = true;
    m_last_auto = auto_clean;
    m_last_success = result.success;
    m_last_fail_status = result.fail_status;

    //显示整理结果
    if (m_result_mode != RM_NONE)
    {
        UpdateDisplayText();

        //弹出主程序的通知消息（RM_NOTIFY 模式）
        if (m_result_mode == RM_NOTIFY && m_pTrafficMonitor != nullptr
            && m_pTrafficMonitor->GetAPIVersion() >= 1)
        {
            wchar_t freed_str[64];
            wchar_t msg[512];
            FormatBytes(result.freed, freed_str, 64);
            if (result.success)
            {
                swprintf_s(msg, L"内存整理完成：占用 %u%% → %u%%，释放 %s。",
                    result.mem_before, result.mem_after, freed_str);
            }
            else
            {
                swprintf_s(msg, L"内存整理未能完成（错误代码 0x%08X）。\r\n"
                    L"请右键 TrafficMonitor，选择“以管理员身份运行”后重试。",
                    result.fail_status);
            }
            m_pTrafficMonitor->ShowNotifyMessage(msg);
        }
    }

    m_cleaning = false;
}

void CPluginMain::CheckAutoClean()
{
    if (!m_auto_enable)
        return;

    //节流：每5秒检查一次
    unsigned long long now = ::GetTickCount64();
    if (m_last_auto_check_tick != 0 && now - m_last_auto_check_tick < 5000)
        return;
    m_last_auto_check_tick = now;

    //条件1：内存占用达到阈值
    if (m_mem_percent < m_auto_threshold)
        return;

    //条件2：距上次整理至少间隔设定的分钟数（防止内存持续偏高时反复整理）
    if (m_last_clean_tick != 0)
    {
        unsigned long long elapsed = now - m_last_clean_tick;
        if (elapsed < (unsigned long long)m_auto_interval * 60000ULL)
            return;
    }

    DoClean(true);
}

void CPluginMain::UpdateDisplayText()
{
    //显示项的数值：
    //整理后的一段时间内显示释放量（时长可在选项中设置，默认5秒），
    //超时后自动恢复为当前内存占用百分比
    unsigned long long hold_ms = (unsigned long long)m_result_hold_seconds * 1000ULL;
    unsigned long long now = ::GetTickCount64();
    if (m_has_cleaned && m_result_mode != RM_NONE
        && (now - m_last_clean_tick) < hold_ms)
    {
        wchar_t freed_str[64];
        FormatBytes(m_last_freed, freed_str, 64);
        m_value_text = std::wstring(L"释放") + freed_str;
    }
    else
    {
        wchar_t buff[16];
        swprintf_s(buff, L"%d%%", m_mem_percent);
        m_value_text = buff;
    }

    //鼠标提示文本
    wchar_t buff[512];
    swprintf_s(buff, L"内存整理：当前占用 %d%%", m_mem_percent);
    m_tooltip_text = buff;
    if (m_has_cleaned)
    {
        wchar_t freed_str[64];
        FormatBytes(m_last_freed, freed_str, 64);
        swprintf_s(buff, L"\r\n上次整理：%s，占用 %u%% → %u%%，释放 %s",
            m_last_auto ? L"自动" : L"手动",
            m_last_mem_before, m_last_mem_after, freed_str);
        m_tooltip_text += buff;
        if (!m_last_success)
        {
            m_tooltip_text += L"\r\n上次整理失败，请以管理员身份运行 TrafficMonitor";
        }
    }
    else if (!MemCleaner::IsElevated())
    {
        m_tooltip_text += L"\r\n提示：请以管理员身份运行 TrafficMonitor 才能整理内存";
    }
}

void CPluginMain::FormatBytes(unsigned long long bytes, wchar_t* buff, size_t len)
{
    if (bytes >= 1024ULL * 1024 * 1024)
        swprintf_s(buff, len, L"%.2f GB", bytes / (1024.0 * 1024 * 1024));
    else if (bytes >= 1024ULL * 1024)
        swprintf_s(buff, len, L"%.1f MB", bytes / (1024.0 * 1024));
    else if (bytes >= 1024ULL)
        swprintf_s(buff, len, L"%.1f KB", bytes / 1024.0);
    else
        swprintf_s(buff, len, L"%llu B", bytes);
}
