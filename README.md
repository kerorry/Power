# Power — 权限工具箱

一个基于 Win32 的小型 GUI 工具，用于**以管理员或 SYSTEM 身份启动常用程序**，以及**对任意文件进行所有者修改、ACL 替换和强制删除**等需要高权限的运维操作。

---

## 功能

- **启动程序**
  - CMD、注册表编辑器、任务管理器、计算器、控制面板、记事本、资源管理器、系统配置实用程序

- **文件操作**页
  - 更改所有者为 **Administrators** 或 **SYSTEM**
  - 将 DACL **替换**为仅含目标 SID 的完全控制
  - **立即强制删除**文件
  - 安排**重启后删除**文件

---

## 构建

- Windows 10 / 11
- Visual Studio（含 MSVC x64 工具链与 Windows SDK）
- 可选：`upx`（用于压缩最终产物；缺失时只警告）

### 修改 `build.bat` 中的 VS 路径

`build.bat` 默认调用：

```bat
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
```

若安装路径不同（例如 VS 2022 实际路径为 `...\2022\Community\...`），请先改路径。

### 编译

```bat
build.bat
```

生成可执行程序

---

## 命令行参数

| 参数 | 行为 |
| --- | --- |
| `--mode=admin --show-toolbox` | 直接以管理员模式打开工具箱 |
| `--mode=system --show-toolbox` | 直接以 SYSTEM 模式打开工具箱（需具备复制令牌的能力） |
| `--mode=admin` / `--mode=system` | 以对应身份启动 `cmd.exe`|
| 无参数 | 弹出权限选择对话框 |
| 无参数 + 非管理员 | 通过 `ShellExecuteW(L"runas", ...)` 触发 UAC 重新启动自身 |

---

## 关键实现说明

- **SYSTEM 令牌获取**：`security.c` 先启用 `SeDebugPrivilege`，定位 `winlogon.exe` 的 PID，`OpenProcessToken` + `DuplicateTokenEx(TokenPrimary)` 得到可用的主令牌。
- **文件操作中的身份切换**：`fileops.c` 在进程本身不是 SYSTEM 时，使用 `ImpersonateLoggedOnUser` 模拟 SYSTEM，以便 access check 在 SYSTEM 上下文下通过；操作完成后 `RevertToSelf`。
- **替换 DACL 的语义**：`ReplaceFullControlForMode` 会先用 `SetEntriesInAclW` 只生成**一条** `GENERIC_ALL` 的 ACE，再以 `DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION` 写回，即**切断继承并移除所有既有条目**（包括 SYSTEM 和当前属主）。执行前会弹出确认框，若属主将失去访问权限会额外提示。
- **格式化**：所有界面字符串消息使用 `wvnsprintfW` 与 `StringCchCatW` 做有界格式化，不依赖 CRT 宽字符格式化；`RunProgram` 的 `args` 只作为数据参与格式化，避免其中的 `%` 被当作格式说明符。
- **句柄安全**：`AdjustTokenPrivileges` 之后额外检查 `GetLastError()`，因为即使返回 `TRUE` 也可能未真正启用特权；`CreateProcess*` 失败时不会关闭 `pi` 中未定义的句柄。

---

## 使用须知与风险提示

> ⚠️ **请仅在你有明确授权的机器上运行本工具。**

- 本工具会**请求 SYSTEM 级令牌**、**替换目标文件的 DACL**、**强制删除文件**。这些操作**不可撤销**，可能造成系统或数据损坏。
- "替换为仅 XXX 完全控制" 会**切断继承**，可能使其他账户（包括 SYSTEM 自身与文件属主）失去访问权限。
- "立即强制删除" 在删除失败时仅返回错误码，不做回收站处理。
- "重启后删除" 依赖 `MoveFileExW(MOVEFILE_DELAY_UNTIL_REBOOT)`，需要目标文件在重启时处于可删除状态。
- 若杀毒软件/EDR 将 `SeDebugPrivilege`、`winlogon.exe` 令牌复制、DACL 改写判定为可疑行为，属于预期现象。

---

## 已知限制

- 仅支持 x64 构建（`build.bat` 调用 `vcvars64.bat`）。
- 对 `cmd.exe` 的"窗口标题"参数依赖 `IDS_CMD_ARGS` 资源串，其余可执行文件不接受自定义参数。
- 布局常量（按钮位置、Tab 高度等）固定在 `common.h` 中，不响应 DPI 缩放；如需适配高 DPI，请在 `power.rc` 中添加 manifest 并调整布局逻辑。
