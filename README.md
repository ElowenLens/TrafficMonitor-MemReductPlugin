# TrafficMonitor 内存整理插件（MemReductPlugin）

为 [TrafficMonitor](https://github.com/zhongyang219/TrafficMonitor)（1.85 及以上版本）开发的 C++ DLL 插件，参考 [henrypp/memreduct](https://github.com/henrypp/memreduct) 的实现思路，通过 Windows NT 原生接口一键释放内存。**全部界面、菜单与提示文本均为简体中文。**

## ✨ 功能特性

- **五种可勾选的释放项目**：
  - 系统工作集
  - 所有进程的工作集
  - 备用列表（Standby List）
  - 低优先级备用页（优先级 0）
  - 系统文件缓存
- **一键触发**：
  - 双击「内存整理」显示项立即整理（无需确认）
  - 右键菜单命令「立即整理内存」（需 TrafficMonitor 1.85+）
- **整理结果展示**：显示整理前/后内存占用百分比与释放量，鼠标悬停可查看详情；释放量显示时长可自定义（3~600 秒，默认 5 秒后自动恢复为占用百分比）
- **自动整理**：内存占用超过设定阈值时自动触发，并可限制两次整理的最小间隔
- **全中文选项设置对话框**（纯 Win32 实现，无 MFC 依赖）
- **配置持久化**：保存在 TrafficMonitor 配置目录下的 `plugins\MemReductPlugin.ini`

## ⚠️ 使用前提（重要）

**TrafficMonitor 必须以管理员身份运行。** 清理命令需要 `SeProfileSingleProcessPrivilege` 和 `SeIncreaseQuotaPrivilege` 两个特权，普通权限下内核返回 `0xC0000061`（STATUS_PRIVILEGE_NOT_HELD），清理不会生效。

插件位数必须与 TrafficMonitor 主程序位数一致（64 位主程序用 x64 DLL，32 位主程序用 Win32 DLL）。

## 📦 安装

1. 下载或编译得到对应位数的 `MemReductPlugin.dll`
2. 放入 TrafficMonitor 主程序目录下的 `plugins` 文件夹（没有就新建一个）：

   ```
   TrafficMonitor.exe 所在目录\
   └── plugins\
       └── MemReductPlugin.dll
   ```

3. 重启 TrafficMonitor（以管理员身份运行）
4. 右键菜单 →「其他功能」→「插件管理」，应能看到「内存整理」插件（作者 ElowenLens）
5. 右键菜单 →「显示设置」→ 勾选「内存整理」显示项
6. **双击该显示项即可触发一次内存整理**；或在插件管理里通过「选项」按钮打开设置对话框

> **关于「双击动作」下拉框的说明**：TrafficMonitor 主程序的「选项设置 → 双击动作」下拉框只有 8 个固定选项，不支持插件命令（主程序机制限制）。官方等效路径是：把插件显示项加到主窗口/任务栏后**双击该显示项**，主程序会调用插件的 `OnMouseEvent(MT_DBCLICKED)`，效果等同。

## 🔨 编译

### 准备

- Visual Studio 2022（含 C++ 桌面开发工作负载），或 MSVC BuildTools + Windows SDK
- Python 3（仅首次生成图标时需要，标准库实现，无第三方依赖）

### 步骤

1. 打开 `MemReductPlugin.sln`，选择配置（如 `Release | x64`），生成解决方案；
   或使用命令行：

   ```bat
   MSBuild.exe MemReductPlugin.sln -p:Configuration=Release -p:Platform=x64 -m
   ```

2. 产物输出到 `bin\平台\配置\MemReductPlugin.dll`

> 图标 `plugin.ico` 已随仓库提供；如需重新生成（例如更换设计），运行 `python make_icon.py` 即可（纯标准库实现，无第三方依赖）。

### 编译要点

- 工具集 `v143`（VS2022）；若用 VS2019，把 vcxproj 里的 `PlatformToolset` 改为 `v142`
- 源码统一用 `/utf-8` 编译；`.rc` 资源文件为带 BOM 的 UTF-8 编码
- 运行库 `/MT` 静态链接，产物不依赖 VC++ Redistributable
- 四个配置（x64/Win32 × Debug/Release）均已验证编译通过（MSVC 14.44 + Windows SDK 10.0.26100）

## ⚙️ 选项设置说明

在「插件管理 → 内存整理 → 选项」中打开全中文设置对话框：

| 设置组 | 内容 |
| --- | --- |
| 要释放的项目 | 五个清理项可任意勾选组合（默认勾选：备用列表、低优先级备用页、系统文件缓存；系统/进程工作集默认不勾选） |
| 整理结果显示方式 | 显示在显示项中（默认）/ 同时弹出主程序通知 / 不显示 |
| 释放量保持时长 | 整理后显示「释放 X.XX GB」的持续秒数（3~600 秒，默认 5 秒） |
| 自动整理 | 启用开关、占用阈值（%）、两次整理最小间隔（分钟） |
| 立即整理（测试） | 保存前即可验证，弹窗显示整理前/后占用与释放量 |

## 🛠️ 技术实现

内存清理通过 `ntdll.dll` 的 NT 原生接口完成（动态加载，兼容 x86/x64）：

| 清理项 | 接口调用 |
| --- | --- |
| 系统/进程工作集 | `NtSetSystemInformation(SystemMemoryListInformation=80, MemoryEmptyWorkingSets=2)` |
| 备用列表 | `NtSetSystemInformation(80, MemoryPurgeStandbyList=4)` |
| 低优先级备用页 | `NtSetSystemInformation(80, MemoryPurgeLowPriorityStandbyList=5)` |
| 系统文件缓存 | `NtSetSystemInformation(SystemFileCacheInformationEx=81)`（最小/最大工作集设为 MAXSIZE_T 触发收缩） |

命令枚举值取自 memreduct 所用的 routine 库，并与当前 [phnt](https://github.com/winsiderss/phnt) 官方枚举核对一致；已在 Windows 11（管理员权限）实测：未启用特权时返回 `0xC0000061`，启用 `SeProfileSingleProcessPrivilege`（`RtlAdjustPrivilege(13)`）后全部生效，工作集清理实测使内存占用 90% → 44%。

备用列表与系统文件缓存中存放的是干净的文件缓存页，清空后数据无损失，仅在下次访问时需重新从磁盘读取，属安全的内存释放方式。

### 目录结构

```
MemReductPlugin/
├── MemReductPlugin.sln                    VS2022 解决方案
├── 编译与使用说明.md                       详细文档
└── plugin/
    ├── MemReductPlugin.vcxproj            项目文件（v143，Unicode，/MT）
    ├── include/
    │   └── PluginInterface.h              官方插件 SDK 头文件（API 版本 8）
    └── src/
        ├── dllmain.cpp / dllmain.h        DLL 入口 + 唯一导出函数 TMPluginGetInstance
        ├── PluginMain.h / PluginMain.cpp  ITMPlugin 主类 + IPluginItem 显示项
        ├── MemCleaner.h / MemCleaner.cpp  NT 原生接口内存清理模块
        ├── OptionsDialog.h / OptionsDialog.cpp  中文选项对话框（纯 Win32）
        ├── resource.h / MemReductPlugin.rc  资源定义 / 图标+对话框+版本信息
        └── make_icon.py                   图标生成脚本（可复现，纯标准库）
```

## 📋 兼容性

| 项目 | 要求 |
| --- | --- |
| TrafficMonitor | 1.85 及以上版本（插件命令菜单为 1.85 新增；API 版本 8） |
| 操作系统 | Windows 10 / 11（NT 接口在 Win7+ 可用，但主程序要求 Win10+） |
| 运行权限 | 必须以管理员身份运行 TrafficMonitor |
| 运行库 | /MT 静态链接，无额外依赖 |

## 📄 许可证

本项目基于 [MIT License](LICENSE) 开源。
[TrafficMonitor](https://github.com/zhongyang219/TrafficMonitor) 与 [memreduct](https://github.com/henrypp/memreduct) 版权归其各自作者所有。
