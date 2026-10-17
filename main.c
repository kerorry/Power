// main.c

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <strsafe.h>

#include "common.h"
#include "security.h"
#include "process.h"
#include "ui.h"

// 启动时重新构造自身命令行的缓冲区容量。
// 最坏情况：路径 259 + 引号 2 + "--mode=system" 13 + "--show-toolbox" 14 + 空格 2 = 290 字符。
#define RELAUNCH_CMD_CHARS (MAX_PATH + 64)

// 显示权限选择对话框，返回 IDM_* 之一
static int ShowMenuDialog(void) {
    WCHAR title[128] = {0}, instr[256] = {0}, content[256] = {0};
    WCHAR textAdmin[64] = {0}, textSystem[64] = {0}, textExit[64] = {0};

    LoadStringW(GetModuleHandle(NULL), IDS_APP_TITLE, title, ARRAYSIZE(title));
    LoadStringW(GetModuleHandle(NULL), IDS_MENU_TITLE, instr, ARRAYSIZE(instr));
    LoadStringW(GetModuleHandle(NULL), IDS_MENU_CONTENT, content, ARRAYSIZE(content));
    LoadStringW(GetModuleHandle(NULL), IDS_MENU_BTN_ADMIN, textAdmin, ARRAYSIZE(textAdmin));
    LoadStringW(GetModuleHandle(NULL), IDS_MENU_BTN_SYSTEM, textSystem, ARRAYSIZE(textSystem));
    LoadStringW(GetModuleHandle(NULL), IDS_MENU_BTN_EXIT, textExit, ARRAYSIZE(textExit));

    TASKDIALOG_BUTTON buttons[] = {
        { IDM_MODE_ADMIN,  textAdmin  },
        { IDM_MODE_SYSTEM, textSystem },
        { IDM_EXIT,        textExit   }
    };

    TASKDIALOGCONFIG config = {0};
    config.cbSize = sizeof(config);
    config.hInstance = GetModuleHandle(NULL);
    config.pszWindowTitle = title;
    config.pszMainInstruction = instr;
    config.pszContent = content;
    config.pButtons = buttons;
    config.cButtons = ARRAYSIZE(buttons);
    config.dwFlags = TDF_USE_COMMAND_LINKS;

    int nButton = 0;
    HRESULT hr = TaskDialogIndirect(&config, &nButton, NULL, NULL);
    if (FAILED(hr) ||
        (nButton != IDM_MODE_ADMIN && nButton != IDM_MODE_SYSTEM && nButton != IDM_EXIT)) {
        MessageBoxRes(NULL, IDS_ERR_TASKDIALOG, MB_ICONERROR, IDS_CAPTION_ERROR);
        ExitProcess(1);
    }
    return nButton;
}

// 以 SYSTEM 令牌重启自身并直接打开工具箱。成功返回 TRUE。
static BOOL RelaunchAsSystem(HANDLE hSysToken) {
    WCHAR path[MAX_PATH] = {0};
    if (GetModuleFileNameW(NULL, path, MAX_PATH) == 0) {
        return FALSE;
    }

    WCHAR cmd[RELAUNCH_CMD_CHARS];
    if (FAILED(StringCchPrintfW(cmd, ARRAYSIZE(cmd), L"\"%s\" --mode=system --show-toolbox", path))) {
        return FALSE;
    }

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {0};
    if (!CreateProcessWithTokenW(hSysToken, 0, NULL, cmd, 0, NULL, NULL, &si, &pi)) {
        return FALSE;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return TRUE;
}

// 通过 UAC 以管理员身份重启自身
static void RelaunchElevated(int mode) {
    WCHAR path[MAX_PATH] = {0};
    if (GetModuleFileNameW(NULL, path, MAX_PATH) == 0) {
        MessageBoxRes(NULL, IDS_ERR_UAC, MB_ICONERROR, IDS_CAPTION_ERROR);
        return;
    }

    WCHAR cmd[RELAUNCH_CMD_CHARS];
    if (FAILED(StringCchPrintfW(cmd, ARRAYSIZE(cmd), L"\"%s\" --mode=%ls --show-toolbox",
                                path, (mode == MODE_ADMIN) ? L"admin" : L"system"))) {
        MessageBoxRes(NULL, IDS_ERR_LAUNCH, MB_ICONERROR, IDS_CAPTION_ERROR);
        return;
    }

    if ((INT_PTR)ShellExecuteW(NULL, L"runas", path, cmd, NULL, SW_SHOWNORMAL) <= 32) {
        MessageBoxRes(NULL, IDS_ERR_UAC, MB_ICONERROR, IDS_CAPTION_ERROR);
    }
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, int nShow) {
    // 这四个参数都不使用：命令行统一从 GetCommandLineW() 读取（避免 ANSI 转换），
    // 窗口显示由 ShowToolbox 的对话框自行处理。
    UNREFERENCED_PARAMETER(hInst);
    UNREFERENCED_PARAMETER(hPrev);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nShow);

    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_TAB_CLASSES };
    InitCommonControlsEx(&icex);

    int mode = 0;
    BOOL showToolbox = FALSE;
    LPCWSTR cmdLine = GetCommandLineW();
    if (cmdLine) {
        if (wcsstr(cmdLine, L"--mode=admin")) {
            mode = MODE_ADMIN;
        } else if (wcsstr(cmdLine, L"--mode=system")) {
            mode = MODE_SYSTEM;
        }
        if (wcsstr(cmdLine, L"--show-toolbox")) {
            showToolbox = TRUE;
        }
    }

    // 已带模式与工具箱参数：直接显示
    if (mode && showToolbox) {
        ShowToolbox(mode);
        return 0;
    }

    // 只带模式：启动 cmd（兼容旧行为）
    if (mode) {
        RunProgram(mode, L"cmd.exe", NULL);
        return 0;
    }

    // 首次启动：选择权限身份
    int choice = ShowMenuDialog();
    if (choice == IDM_EXIT) {
        return 0;
    }
    mode = (choice == IDM_MODE_ADMIN) ? MODE_ADMIN : MODE_SYSTEM;

    // 选了 SYSTEM 但当前不是 SYSTEM：尝试用 SYSTEM 令牌重启自身
    if (mode == MODE_SYSTEM && !IsSystem()) {
        HANDLE hSysToken = GetSystemToken();
        if (hSysToken) {
            BOOL ok = RelaunchAsSystem(hSysToken);
            CloseHandle(hSysToken);
            if (ok) {
                return 0;
            }
        }
        // 取不到令牌或启动失败：退回管理员模式
        MessageBoxRes(NULL, IDS_SYSTEM_TOKEN_FAILED, MB_ICONERROR, IDS_CAPTION_ERROR);
        mode = MODE_ADMIN;
    }

    // 已是管理员：直接显示；否则按原逻辑提权
    if (IsAdmin()) {
        ShowToolbox(mode);
    } else {
        RelaunchElevated(mode);
    }
    return 0;
}
