// ui.c

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <stdlib.h>

#include "ui.h"
#include "common.h"
#include "process.h"
#include "fileops.h"
#include "security.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// 启动程序页的控件 ID，顺序必须与 CMDS[] 一致
static const int kRunButtonIds[BUTTON_COUNT] = {
    IDC_CMD, IDC_REGEDIT, IDC_TASKMGR, IDC_CALC,
    IDC_CONTROL, IDC_NOTEPAD, IDC_EXPLORER, IDC_MSCONFIG
};

// 文件操作页的控件 ID 与对应资源串，顺序一致
static const int kFileButtonIds[] = {
    IDC_OWNER_ADMIN, IDC_OWNER_SYSTEM,
    IDC_ADD_PERM_ADMIN, IDC_ADD_PERM_SYSTEM,
    IDC_FORCE_DELETE, IDC_DELETE_ON_REBOOT
};
static const int kFileButtonRes[] = {
    IDS_OWNER_ADMIN, IDS_OWNER_SYSTEM,
    IDS_ADD_PERM_ADMIN, IDS_ADD_PERM_SYSTEM,
    IDS_FORCE_DELETE, IDS_DELETE_ON_REBOOT
};

// 启动程序页的按钮标题资源串（对话框模板里的文字只作设计期占位）
static const int kRunButtonRes[BUTTON_COUNT] = {
    IDS_CMD_TEXT, IDS_REGEDIT_TEXT, IDS_TASKMGR_TEXT, IDS_CALC_TEXT,
    IDS_CONTROL_TEXT, IDS_NOTEPAD_TEXT, IDS_EXPLORER_TEXT, IDS_MSCONFIG_TEXT
};

#define FILE_BUTTON_COUNT ARRAYSIZE(kFileButtonIds)

static INT_PTR CALLBACK ToolboxProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);

// 启动程序页的可滚动内容总高度
static int RunPageContentHeight(void) {
    return BUTTON_Y_START + BUTTON_COUNT * BUTTON_STEP + BUTTON_HEIGHT + 10;
}

// 按客户区宽度重新居中并纵向排布启动程序按钮
static void UpdateButtonPositions(HWND hDlg, int scrollPos) {
    RECT rc;
    GetClientRect(hDlg, &rc);

    int cx = (rc.right - BUTTON_WIDTH) / 2;
    if (cx < 0) {
        cx = 0;
    }

    for (int i = 0; i < BUTTON_COUNT; i++) {
        HWND hBtn = GetDlgItem(hDlg, kRunButtonIds[i]);
        if (hBtn) {
            int y = BUTTON_Y_START + i * BUTTON_STEP - scrollPos;
            SetWindowPos(hBtn, NULL, cx, y, BUTTON_WIDTH, BUTTON_HEIGHT, SWP_NOZORDER);
        }
    }
}

// 依据当前标签页刷新滚动条并同步按钮位置
static void RefreshScrollBar(HWND hDlg, int currentTab, int* pScrollPos, int* pMaxScroll) {
    RECT rc;
    GetClientRect(hDlg, &rc);

    if (currentTab == 0) {
        int totalHeight = RunPageContentHeight();
        int clientHeight = rc.bottom - rc.top;

        *pMaxScroll = max(0, totalHeight - clientHeight);
        SCROLLINFO si = { sizeof(si), SIF_RANGE | SIF_PAGE, 0, totalHeight, clientHeight };
        SetScrollInfo(hDlg, SB_VERT, &si, TRUE);

        if (*pScrollPos > *pMaxScroll) {
            *pScrollPos = *pMaxScroll;
        }
        SetScrollPos(hDlg, SB_VERT, *pScrollPos, TRUE);
        UpdateButtonPositions(hDlg, *pScrollPos);
    } else {
        SCROLLINFO si = { sizeof(si), SIF_RANGE | SIF_PAGE, 0, 0, 0 };
        SetScrollInfo(hDlg, SB_VERT, &si, TRUE);
    }
}

// 选择要操作的文件
static BOOL SelectFile(HWND hDlg, wchar_t* szFile, DWORD nMaxFile) {
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hDlg;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = nMaxFile;
    ofn.lpstrFilter = L"all files\0*.*\0";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    return GetOpenFileNameW(&ofn);
}

// ---------- 文件操作按钮的处理函数 ----------
static void OnChangeFileOwner(HWND hDlg, int mode) {
    wchar_t szFile[MAX_PATH] = {0};
    if (SelectFile(hDlg, szFile, MAX_PATH)) {
        ChangeOwnerForMode(mode, szFile);
    }
}

static void OnReplaceFullControl(HWND hDlg, int mode) {
    wchar_t szFile[MAX_PATH] = {0};
    if (SelectFile(hDlg, szFile, MAX_PATH)) {
        ReplaceFullControlForMode(mode, szFile);
    }
}

static void OnForceDeleteNow(HWND hDlg) {
    wchar_t szFile[MAX_PATH] = {0};
    if (SelectFile(hDlg, szFile, MAX_PATH)) {
        ForceDeleteFileNow(hDlg, szFile);
    }
}

static void OnDeleteOnReboot(HWND hDlg) {
    wchar_t szFile[MAX_PATH] = {0};
    if (SelectFile(hDlg, szFile, MAX_PATH)) {
        DeleteFileOnReboot(hDlg, szFile);
    }
}

// ---------- 对话框过程 ----------
static INT_PTR CALLBACK ToolboxProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static int  g_mode = MODE_ADMIN;
    static int  nScrollPos = 0;
    static int  nMaxScroll = 0;
    static int  currentTab = 0;
    static HWND hTab = NULL;
    static HWND hFileButtons[FILE_BUTTON_COUNT] = {0};

    switch (msg) {
        case WM_INITDIALOG: {
            g_mode = (int)(INT_PTR)lParam;
            SetWindowTextRes(hDlg, IDS_APP_TITLE);

            HICON hIcon = (HICON)LoadImageW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDI_ICON1),
                                            IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
            if (hIcon) {
                SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
                SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
            }

            RECT rc;
            GetClientRect(hDlg, &rc);
            HFONT hFont = (HFONT)SendMessage(hDlg, WM_GETFONT, 0, 0);

            hTab = CreateWindowW(L"SysTabControl32", L"",
                                 WS_CHILD | WS_VISIBLE | TCS_FIXEDWIDTH,
                                 10, 10, rc.right - 20, TAB_HEIGHT,
                                 hDlg, (HMENU)IDC_TAB, GetModuleHandle(NULL), NULL);
            SendMessage(hTab, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));

            TCITEMW tie = {0};
            tie.mask = TCIF_TEXT;
            WCHAR tabText[64];

            if (LoadStringW(GetModuleHandle(NULL), IDS_TAB1, tabText, ARRAYSIZE(tabText))) {
                tie.pszText = tabText;
                TabCtrl_InsertItem(hTab, 0, &tie);
            }
            if (LoadStringW(GetModuleHandle(NULL), IDS_TAB2, tabText, ARRAYSIZE(tabText))) {
                tie.pszText = tabText;
                TabCtrl_InsertItem(hTab, 1, &tie);
            }

            // 启动程序页的按钮标题统一从资源串设置
            for (int i = 0; i < BUTTON_COUNT; i++) {
                SetWindowTextRes(GetDlgItem(hDlg, kRunButtonIds[i]), kRunButtonRes[i]);
            }

            nScrollPos = 0;
            RefreshScrollBar(hDlg, currentTab, &nScrollPos, &nMaxScroll);

            // 创建文件操作页的六个按钮；初始隐藏，切到该页才显示
            int btnY = 60;
            int btnCX = (rc.right - 200) / 2;
            const int btnWidth = 200, btnHeight = 30, btnStep = 40;

            for (int i = 0; i < FILE_BUTTON_COUNT; i++) {
                hFileButtons[i] = CreateWindowW(L"BUTTON", L"",
                                                WS_CHILD | BS_PUSHBUTTON,
                                                btnCX, btnY, btnWidth, btnHeight,
                                                hDlg, (HMENU)(INT_PTR)kFileButtonIds[i],
                                                GetModuleHandle(NULL), NULL);
                SendMessage(hFileButtons[i], WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
                SetWindowTextRes(hFileButtons[i], kFileButtonRes[i]);
                btnY += btnStep;
            }
            return TRUE;
        }

        case WM_SIZE:
            if (hTab) {
                RECT rc;
                GetClientRect(hDlg, &rc);
                SetWindowPos(hTab, NULL, 10, 10, rc.right - 20, TAB_HEIGHT, SWP_NOZORDER);

                nScrollPos = 0;
                RefreshScrollBar(hDlg, currentTab, &nScrollPos, &nMaxScroll);
            }
            break;

        case WM_VSCROLL: {
            if (currentTab != 0) {
                break;
            }

            int newPos = nScrollPos;
            switch (LOWORD(wParam)) {
                case SB_LINEUP:     newPos -= BUTTON_STEP;     break;
                case SB_LINEDOWN:   newPos += BUTTON_STEP;     break;
                case SB_PAGEUP:     newPos -= BUTTON_STEP * 4; break;
                case SB_PAGEDOWN:   newPos += BUTTON_STEP * 4; break;
                case SB_THUMBTRACK: newPos = HIWORD(wParam);   break;
                default:            return FALSE;
            }

            newPos = max(0, min(newPos, nMaxScroll));
            if (newPos == nScrollPos) {
                break;
            }
            nScrollPos = newPos;
            SetScrollPos(hDlg, SB_VERT, nScrollPos, TRUE);
            UpdateButtonPositions(hDlg, nScrollPos);
            return TRUE;
        }

        case WM_NOTIFY: {
            NMHDR* pnmh = (NMHDR*)lParam;
            if (!hTab || pnmh->idFrom != IDC_TAB || pnmh->code != TCN_SELCHANGE) {
                break;
            }

            int newTab = TabCtrl_GetCurSel(hTab);
            if (newTab != currentTab) {
                currentTab = newTab;
                BOOL showRun = (currentTab == 0);

                for (int i = 0; i < BUTTON_COUNT; i++) {
                    ShowWindow(GetDlgItem(hDlg, kRunButtonIds[i]), showRun ? SW_SHOW : SW_HIDE);
                }
                for (int i = 0; i < FILE_BUTTON_COUNT; i++) {
                    ShowWindow(hFileButtons[i], showRun ? SW_HIDE : SW_SHOW);
                }

                nScrollPos = 0;
                RefreshScrollBar(hDlg, currentTab, &nScrollPos, &nMaxScroll);
            }
            return TRUE;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);

            if (id == IDOK || id == IDCANCEL) {
                EndDialog(hDlg, 0);
                return TRUE;
            }

            // 只处理按钮点击，避免其他通知码误触发
            if (HIWORD(wParam) != BN_CLICKED) {
                return FALSE;
            }

            if (id >= IDC_CMD && id <= IDC_MSCONFIG) {
                static const LPCWSTR kRunExe[] = {
                    L"cmd.exe", L"regedit.exe", L"taskmgr.exe", L"calc.exe",
                    L"control.exe", L"notepad.exe", L"explorer.exe", L"msconfig.exe"
                };

                // cmd.exe 的窗口标题参数也走资源串，便于统一本地化
                static WCHAR cmdArgs[128] = {0};
                if (!cmdArgs[0]) {
                    LoadStringW(GetModuleHandle(NULL), IDS_CMD_ARGS, cmdArgs, ARRAYSIZE(cmdArgs));
                }

                int idx = id - IDC_CMD;
                if (idx < ARRAYSIZE(kRunExe)) {
                    // 只有 cmd.exe 需要额外参数
                    LPCWSTR args = (idx == 0 && cmdArgs[0]) ? cmdArgs : NULL;
                    RunProgram(g_mode, kRunExe[idx], args);
                }
                return TRUE;
            }

            switch (id) {
                case IDC_OWNER_ADMIN:      OnChangeFileOwner(hDlg, MODE_ADMIN);     return TRUE;
                case IDC_OWNER_SYSTEM:     OnChangeFileOwner(hDlg, MODE_SYSTEM);    return TRUE;
                case IDC_ADD_PERM_ADMIN:   OnReplaceFullControl(hDlg, MODE_ADMIN);  return TRUE;
                case IDC_ADD_PERM_SYSTEM:  OnReplaceFullControl(hDlg, MODE_SYSTEM); return TRUE;
                case IDC_FORCE_DELETE:     OnForceDeleteNow(hDlg);                  return TRUE;
                case IDC_DELETE_ON_REBOOT: OnDeleteOnReboot(hDlg);                  return TRUE;
                default: break;
            }
            return FALSE;
        }
    }
    return FALSE;
}

void ShowToolbox(int mode) {
    DialogBoxParamW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_TOOLBOX),
                    NULL, ToolboxProc, (LPARAM)mode);
}
