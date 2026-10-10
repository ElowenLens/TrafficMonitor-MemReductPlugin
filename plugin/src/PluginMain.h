/*************************************************************************
* MemReductPlugin - TrafficMonitor 内存整理插件
* PluginMain.h - 插件主类（ITMPlugin）与显示项类（IPluginItem）声明
*************************************************************************/
#pragma once
#include <windows.h>
#include "PluginInterface.h"
#include <string>

class CPluginMain;

//=========================================================================
//显示项：在主窗口/任务栏显示内存占用或上次释放量，
//双击该显示项即可立即执行一次内存整理
//=========================================================================
class CMemoryItem : public IPluginItem
{
public:
    explicit CMemoryItem(CPluginMain* plugin) : m_plugin(plugin) {}

    //IPluginItem
    const wchar_t* GetItemName() const override;
    const wchar_t* GetItemId() const override;
    const wchar_t* GetItemLableText() const override;
    const wchar_t* GetItemValueText() const override;
    const wchar_t* GetItemValueSampleText() const override;
    int OnMouseEvent(MouseEventType type, int x, int y, void* hWnd, int flag) override;

private:
    CPluginMain* m_plugin;
};

//=========================================================================
//插件主类
//=========================================================================
class CPluginMain : public ITMPlugin
{
public:
    static CPluginMain& Instance();

    CPluginMain();
    ~CPluginMain() = default;
    CPluginMain(const CPluginMain&) = delete;
    CPluginMain& operator=(const CPluginMain&) = delete;

    //---------------- ITMPlugin 接口 ----------------
    IPluginItem* GetItem(int index) override;
    void DataRequired() override;
    OptionReturn ShowOptionsDialog(void* hParent) override;
    const wchar_t* GetInfo(PluginInfoIndex index) override;
    void OnMonitorInfo(const MonitorInfo& monitor_info) override;
    const wchar_t* GetTooltipInfo() override;
    void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;
    void* GetPluginIcon() override;
    int GetCommandCount() override;
    const wchar_t* GetCommandName(int command_index) override;
    void* GetCommandIcon(int command_index) override;
    void OnPluginCommand(int command_index, void* hWnd, void* para) override;
    int IsCommandChecked(int command_index) override;
    void OnInitialize(ITrafficMonitor* pApp) override;

    //---------------- 对外提供的功能 ----------------
    //执行一次内存整理。auto_clean 为 true 时按自动整理的设定处理
    void DoClean(bool auto_clean);

    //配置的读取与保存
    void LoadConfig();
    void SaveConfig();
    void SetConfigDir(const wchar_t* dir);
    const wchar_t* GetConfigDir() const { return m_config_dir.c_str(); }

    //释放项目掩码（MemCleaner::CleanItems 组合）
    unsigned GetCleanMask() const;
    void SetCleanMask(unsigned mask);

    //整理结果显示方式
    enum ResultMode
    {
        RM_ITEM = 0,        //显示在插件显示项中（默认）
        RM_NOTIFY = 1,      //显示项 + 主程序通知消息
        RM_NONE = 2,        //不显示结果
    };
    ResultMode GetResultMode() const { return m_result_mode; }
    void SetResultMode(ResultMode mode) { m_result_mode = mode; }

    //释放量在显示项中的保持时长（秒），超时后恢复为内存占用百分比
    int GetResultHoldSeconds() const { return m_result_hold_seconds; }
    void SetResultHoldSeconds(int seconds) { m_result_hold_seconds = seconds; }

    //自动整理
    bool IsAutoCleanEnabled() const { return m_auto_enable; }
    void SetAutoCleanEnabled(bool enable) { m_auto_enable = enable; }
    int GetAutoThreshold() const { return m_auto_threshold; }       //内存占用阈值(%)
    void SetAutoThreshold(int threshold) { m_auto_threshold = threshold; }
    int GetAutoInterval() const { return m_auto_interval; }         //两次整理最小间隔(分钟)
    void SetAutoInterval(int minutes) { m_auto_interval = minutes; }

    //最近一次整理的信息（用于显示）
    unsigned long long GetLastFreed() const { return m_last_freed; }
    DWORD GetLastMemBefore() const { return m_last_mem_before; }
    DWORD GetLastMemAfter() const { return m_last_mem_after; }
    unsigned long long GetLastCleanTick() const { return m_last_clean_tick; }
    bool HasCleanedOnce() const { return m_has_cleaned; }
    bool IsLastCleanAuto() const { return m_last_auto; }
    bool IsLastCleanSuccess() const { return m_last_success; }
    DWORD GetLastFailStatus() const { return m_last_fail_status; }

    //当前内存占用（来自主程序的监控信息）
    int GetCurrentMemPercent() const { return m_mem_percent; }

    ITrafficMonitor* GetTrafficMonitor() const { return m_pTrafficMonitor; }

    //把字节数格式化成便于阅读的中文文本，如 "1.23 GB"
    static void FormatBytes(unsigned long long bytes, wchar_t* buff, size_t len);

private:
    friend class CMemoryItem;   //显示项需要访问显示文本缓存

    void CheckAutoClean();          //在 DataRequired 中调用，检查是否需要自动整理
    void UpdateDisplayText();       //刷新显示项与提示的文本

    CMemoryItem m_item;
    ITrafficMonitor* m_pTrafficMonitor = nullptr;

    //配置
    std::wstring m_config_dir;
    std::wstring m_config_file;
    unsigned m_clean_mask = 0xFFFF;             //默认全部勾选
    ResultMode m_result_mode = RM_ITEM;
    int m_result_hold_seconds = 5;              //释放量显示保持时长（秒）
    bool m_auto_enable = false;
    int m_auto_threshold = 85;
    int m_auto_interval = 30;
    bool m_config_loaded = false;

    //运行状态
    int m_mem_percent = 0;                      //当前内存占用
    unsigned long long m_last_clean_tick = 0;   //上次整理的时间（GetTickCount64）
    unsigned long long m_last_freed = 0;        //上次释放的字节数
    DWORD m_last_mem_before = 0;
    DWORD m_last_mem_after = 0;
    bool m_has_cleaned = false;
    bool m_last_auto = false;
    bool m_last_success = false;
    DWORD m_last_fail_status = 0;
    bool m_cleaning = false;                    //防止重入
    unsigned long long m_last_auto_check_tick = 0;

    //显示文本缓存
    std::wstring m_value_text;
    std::wstring m_tooltip_text;
};
