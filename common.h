// common.h
#pragma once

#include <windows.h>
#include <commctrl.h>
#include <stdarg.h>
#include <shlwapi.h>

#include "resource.h"

#pragma comment(lib, "shlwapi.lib")

// 运行模式（权限身份）
#define MODE_ADMIN  1
#define MODE_SYSTEM 2

// 按钮布局常量（启动程序标签页）
#define BUTTON_Y_START  50
#define BUTTON_STEP     38
#define BUTTON_COUNT    8
#define BUTTON_HEIGHT   25
#define BUTTON_WIDTH    190
#define TAB_HEIGHT      30

// 某些 SDK 版本可能未定义这些常量，手动补充
#ifndef TOKEN_IMPERSONATION
#define TOKEN_IMPERSONATION 0x0004
#endif

#ifndef SecurityImpersonation
#define SecurityImpersonation 2
#endif

#ifndef TokenPrimary
#define TokenPrimary 1
#endif

// 读取资源串到静态缓冲区，用于消息框标题。失败时返回 NULL（MessageBox 会使用默认标题）。
static inline LPCWSTR CaptionRes(int resId) {
    static WCHAR buf[128];
    return LoadStringW(GetModuleHandle(NULL), resId, buf, ARRAYSIZE(buf)) ? buf : NULL;
}

// 以资源字符串设置窗口标题
static inline void SetWindowTextRes(HWND hWnd, int resId) {
    WCHAR buf[256];
    if (LoadStringW(GetModuleHandle(NULL), resId, buf, ARRAYSIZE(buf))) {
        SetWindowTextW(hWnd, buf);
    }
}

// 弹出资源字符串消息框
static inline void MessageBoxRes(HWND hWnd, int resId, UINT uType, int captionResId) {
    WCHAR buf[256];
    if (LoadStringW(GetModuleHandle(NULL), resId, buf, ARRAYSIZE(buf))) {
        MessageBoxW(hWnd, buf, CaptionRes(captionResId), uType);
    }
}

// 弹出资源字符串消息框，支持 %s / %lu 等格式化参数。
// 用 Win32 的 wvnsprintfW（shlwapi）做有界格式化，不用 CRT 的宽字符格式化函数：
// 后者在 MSVC 下被标记为弃用，而且不做边界检查。
// 注意：wvnsprintfW 不支持 %S，资源串里一律用 %s（宽字符语义）。
static inline void MessageBoxResFmt(HWND hWnd, int resId, UINT uType, int captionResId, ...) {
    WCHAR fmt[512];
    if (!LoadStringW(GetModuleHandle(NULL), resId, fmt, ARRAYSIZE(fmt))) return;

    WCHAR buf[2048];
    va_list args;
    va_start(args, captionResId);
    wvnsprintfW(buf, ARRAYSIZE(buf), fmt, args);
    va_end(args);

    MessageBoxW(hWnd, buf, CaptionRes(captionResId), uType);
}
