// process.c

#include <windows.h>
#include <tlhelp32.h>
#include <strsafe.h>

#include "common.h"
#include "process.h"
#include "security.h"

DWORD FindPidByName(LPCWSTR name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32W pe = { sizeof(pe) };
    DWORD pid = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, name) == 0) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }

    CloseHandle(snap);
    return pid;
}

void RunProgram(int mode, LPCWSTR exePath, LPCWSTR args) {
    // args 只作为数据参与格式化，不能放到格式串位置（否则其中的 % 会被当成转换说明符）
    WCHAR cmdLine[MAX_PATH + 512];
    if (FAILED(StringCchPrintfW(cmdLine, ARRAYSIZE(cmdLine), L"\"%s\" %s",
                                exePath, args ? args : L""))) {
        MessageBoxRes(NULL, IDS_ERR_LAUNCH, MB_ICONERROR, IDS_CAPTION_ERROR);
        return;
    }

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {0};
    BOOL ok = FALSE;

    if (mode == MODE_SYSTEM) {
        HANDLE hToken = GetSystemToken();
        if (!hToken) {
            MessageBoxRes(NULL, IDS_ERR_GET_TOKEN, MB_ICONERROR, IDS_CAPTION_ERROR);
            return;
        }
        ok = CreateProcessWithTokenW(hToken, 0, NULL, cmdLine, CREATE_NEW_CONSOLE,
                                     NULL, NULL, &si, &pi);
        CloseHandle(hToken);
    } else {
        ok = CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE, CREATE_NEW_CONSOLE,
                            NULL, NULL, &si, &pi);
    }

    if (!ok) {
        MessageBoxRes(NULL, IDS_ERR_RUN, MB_ICONERROR, IDS_CAPTION_ERROR);
        return;  // 失败时 pi 内容未定义，不能关闭句柄
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
}
