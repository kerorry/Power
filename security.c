// security.c

#include <windows.h>
#include <sddl.h>
#include <securitybaseapi.h>

#include "common.h"
#include "security.h"
#include "process.h"

// 启用当前进程令牌上的单个特权。
// 注意：AdjustTokenPrivileges 即使成功也可能返回 TRUE 而不真正启用特权
// （例如特权不存在时），因此必须再检查 GetLastError。
static BOOL EnablePrivilege(LPCWSTR name) {
    HANDLE hToken = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return FALSE;
    }

    TOKEN_PRIVILEGES tp = {0};
    if (!LookupPrivilegeValueW(NULL, name, &tp.Privileges[0].Luid)) {
        CloseHandle(hToken);
        return FALSE;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    BOOL ok = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL)
              && (GetLastError() == ERROR_SUCCESS);
    CloseHandle(hToken);
    return ok;
}

BOOL IsAdmin(void) {
    HANDLE hToken = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        return FALSE;
    }

    TOKEN_ELEVATION elevation = {0};
    DWORD cbSize = 0;
    BOOL ok = GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &cbSize);
    CloseHandle(hToken);

    // 查询失败时按"未提权"处理，走 UAC 提权路径（失败关闭）
    return ok ? (elevation.TokenIsElevated != 0) : FALSE;
}

BOOL IsSystem(void) {
    HANDLE hToken = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        return FALSE;
    }

    DWORD cbSize = 0;
    // 首次调用预期返回 FALSE 且 GetLastError() == ERROR_INSUFFICIENT_BUFFER
    GetTokenInformation(hToken, TokenUser, NULL, 0, &cbSize);
    if (cbSize == 0) {
        CloseHandle(hToken);
        return FALSE;
    }

    BYTE* buffer = (BYTE*)LocalAlloc(LPTR, cbSize);
    if (!buffer) {
        CloseHandle(hToken);
        return FALSE;
    }

    BOOL isSystem = FALSE;
    if (GetTokenInformation(hToken, TokenUser, buffer, cbSize, &cbSize)) {
        isSystem = IsWellKnownSid(((TOKEN_USER*)buffer)->User.Sid, WinLocalSystemSid);
    }

    LocalFree(buffer);
    CloseHandle(hToken);
    return isSystem;
}

BOOL EnableSystemTokenPrivileges(void) {
    return EnablePrivilege(L"SeDebugPrivilege");
}

BOOL EnableRestorePrivilege(void) {
    return EnablePrivilege(SE_RESTORE_NAME);
}

BOOL EnableTakeOwnershipPrivilege(void) {
    return EnablePrivilege(SE_TAKE_OWNERSHIP_NAME);
}

HANDLE GetSystemToken(void) {
    if (!EnableSystemTokenPrivileges()) {
        return NULL;
    }

    DWORD pid = FindPidByName(L"winlogon.exe");
    if (!pid) {
        return NULL;
    }

    HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!hProc) {
        return NULL;
    }

    HANDLE hToken = NULL;
    HANDLE hSysToken = NULL;
    if (OpenProcessToken(hProc, TOKEN_DUPLICATE | TOKEN_QUERY | TOKEN_IMPERSONATION, &hToken)) {
        DuplicateTokenEx(hToken, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenPrimary, &hSysToken);
        CloseHandle(hToken);
    }
    CloseHandle(hProc);

    return hSysToken;
}
