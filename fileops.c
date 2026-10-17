// fileops.c

#include <windows.h>
#include <sddl.h>
#include <Aclapi.h>
#include <shlwapi.h>
#include <strsafe.h>

#include "common.h"
#include "fileops.h"
#include "security.h"

#ifndef PROTECTED_DACL_SECURITY_INFORMATION
#define PROTECTED_DACL_SECURITY_INFORMATION 0x80000000
#endif

// 每种权限身份对应的 SID 字符串
static LPCWSTR SidStringForMode(int mode) {
    return (mode == MODE_SYSTEM) ? L"S-1-5-18" : L"S-1-5-32-544";
}

// ---------- 以 SYSTEM 令牌执行操作（模拟） ----------
// 进程本身是管理员令牌时无法直接改写受保护对象的 DACL，
// 需要先模拟 SYSTEM，让 access check 在 SYSTEM 上下文里进行。
static BOOL ImpersonateSystemToken(HANDLE* phToken) {
    *phToken = NULL;
    if (IsSystem()) {
        return TRUE;  // 已经是 SYSTEM，无需模拟
    }

    HANDLE hToken = GetSystemToken();
    if (!hToken) {
        MessageBoxRes(NULL, IDS_SYSTEM_TOKEN_FAILED, MB_ICONERROR, IDS_CAPTION_ERROR);
        return FALSE;
    }
    if (!ImpersonateLoggedOnUser(hToken)) {
        CloseHandle(hToken);
        MessageBoxRes(NULL, IDS_IMPERSONATE_FAILED, MB_ICONERROR, IDS_CAPTION_ERROR);
        return FALSE;
    }

    *phToken = hToken;
    return TRUE;
}

static void RevertSystemToken(HANDLE hToken) {
    if (hToken) {
        RevertToSelf();
        CloseHandle(hToken);
    }
}

// 把 SID 转成可显示的账户名，失败时退回 fallback 文本。
// 只用于确认框的展示，失败不算错误。
static void SidToDisplayName(PSID pSid, LPCWSTR fallback, LPWSTR out, DWORD cchOut) {
    WCHAR name[128] = {0}, domain[128] = {0};
    DWORD cchName = ARRAYSIZE(name), cchDomain = ARRAYSIZE(domain);
    SID_NAME_USE use = SidTypeUnknown;

    if (LookupAccountSidW(NULL, pSid, name, &cchName, domain, &cchDomain, &use) && name[0]) {
        if (domain[0]) {
            wnsprintfW(out, cchOut, L"%s\\%s", domain, name);
        } else {
            wnsprintfW(out, cchOut, L"%s", name);
        }
        return;
    }
    wnsprintfW(out, cchOut, L"%s", fallback ? fallback : L"?");
}

static void SidStringToDisplayName(LPCWSTR sidStr, LPWSTR out, DWORD cchOut) {
    PSID pSid = NULL;
    if (ConvertStringSidToSidW(sidStr, &pSid)) {
        SidToDisplayName(pSid, sidStr, out, cchOut);
        LocalFree(pSid);
    } else {
        wnsprintfW(out, cchOut, L"%s", sidStr);
    }
}

// 读取文件当前所有者。读不到时返回 FALSE，调用方按"没有额外所有者"处理。
static BOOL GetFileOwnerSid(LPCWSTR filePath, PSID* ppSid) {
    PSID pOwner = NULL;
    PSECURITY_DESCRIPTOR pSD = NULL;
    *ppSid = NULL;

    if (GetNamedSecurityInfoW((LPWSTR)filePath, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION,
                              &pOwner, NULL, NULL, NULL, &pSD) != ERROR_SUCCESS || !pOwner) {
        if (pSD) {
            LocalFree(pSD);
        }
        return FALSE;
    }

    // pOwner 指向 pSD 内部，必须按 SID 长度复制出来
    DWORD cb = GetLengthSid(pOwner);
    PSID pCopy = (PSID)LocalAlloc(LPTR, cb);
    if (pCopy) {
        CopySid(cb, pCopy, pOwner);
        *ppSid = pCopy;
    }
    LocalFree(pSD);
    return (*ppSid != NULL);
}

// ---------- 修改所有者 ----------
void ChangeOwnerForMode(int mode, LPCWSTR filePath) {
    HANDLE hToken = NULL;
    if (!ImpersonateSystemToken(&hToken)) {
        return;
    }

    // 以下操作在 SYSTEM 令牌（或本进程已是 SYSTEM）的上下文中执行
    if (!EnableTakeOwnershipPrivilege()) {
        MessageBoxRes(NULL, IDS_TAKE_OWNERSHIP_FAILED, MB_ICONERROR, IDS_CAPTION_ERROR);
        RevertSystemToken(hToken);
        return;
    }
    if (!EnableRestorePrivilege()) {
        MessageBoxRes(NULL, IDS_RESTORE_PRIVILEGE_FAILED, MB_ICONERROR, IDS_CAPTION_ERROR);
        RevertSystemToken(hToken);
        return;
    }

    LPCWSTR sidStr = SidStringForMode(mode);
    PSID pSid = NULL;
    if (!ConvertStringSidToSidW(sidStr, &pSid)) {
        MessageBoxRes(NULL, IDS_CONVERT_SID_FAILED, MB_ICONERROR, IDS_CAPTION_ERROR);
        RevertSystemToken(hToken);
        return;
    }

    DWORD res = SetNamedSecurityInfoW((LPWSTR)filePath, SE_FILE_OBJECT,
                                      OWNER_SECURITY_INFORMATION, pSid, NULL, NULL, NULL);
    LocalFree(pSid);

    BOOL ok = (res == ERROR_SUCCESS);
    MessageBoxRes(NULL, ok ? IDS_OWNER_CHANGED : IDS_OWNER_CHANGE_FAILED,
                  ok ? MB_ICONINFORMATION : MB_ICONERROR,
                  ok ? IDS_CAPTION_SUCCESS : IDS_CAPTION_ERROR);
    RevertSystemToken(hToken);
}

// ---------- 替换 DACL：只保留目标 SID 一条完全控制，并切断继承 ----------
// 语义是"仅 X"：除目标 SID 外的所有条目（含 SYSTEM、文件属主、当前用户）都会被移除，
// 执行前经用户确认；属主将失去访问时会额外提示。
static void ReplaceFullControlForSid(LPCWSTR filePath, PSID pSid, LPCWSTR targetSidStr) {
    if (!EnableTakeOwnershipPrivilege()) {
        MessageBoxRes(NULL, IDS_TAKE_OWNERSHIP_FAILED, MB_ICONERROR, IDS_CAPTION_ERROR);
        return;
    }
    if (!EnableRestorePrivilege()) {
        MessageBoxRes(NULL, IDS_RESTORE_PRIVILEGE_FAILED, MB_ICONERROR, IDS_CAPTION_ERROR);
        return;
    }

    // 取出目标账户名与当前所有者，仅用于确认框展示
    WCHAR targetName[256] = {0};
    SidStringToDisplayName(targetSidStr, targetName, ARRAYSIZE(targetName));

    PSID pOwnerSid = NULL;
    BOOL haveOwner = GetFileOwnerSid(filePath, &pOwnerSid);
    BOOL ownerLosesAccess = haveOwner && !EqualSid(pOwnerSid, pSid);

    WCHAR confirmFmt[1024], ownerFmt[256], targetFmt[256];
    if (!LoadStringW(GetModuleHandle(NULL), IDS_DACL_CONFIRM, confirmFmt, ARRAYSIZE(confirmFmt)) ||
        !LoadStringW(GetModuleHandle(NULL), IDS_DACL_REPLACE_TARGET, targetFmt, ARRAYSIZE(targetFmt))) {
        if (pOwnerSid) {
            LocalFree(pOwnerSid);
        }
        MessageBoxRes(NULL, IDS_CREATE_DACL_FAILED, MB_ICONERROR, IDS_CAPTION_ERROR);
        return;
    }

    // 单条消息用多个资源串拼成，以便控制每个 %s 分别对应哪个字段
    WCHAR msg[4096] = {0};
    wnsprintfW(msg, ARRAYSIZE(msg), confirmFmt, filePath, targetName, targetName);
    StringCchCatW(msg, ARRAYSIZE(msg), targetFmt);
    if (ownerLosesAccess) {
        WCHAR ownerName[256] = {0};
        SidToDisplayName(pOwnerSid, NULL, ownerName, ARRAYSIZE(ownerName));
        if (LoadStringW(GetModuleHandle(NULL), IDS_DACL_REMOVE_OWNER, ownerFmt, ARRAYSIZE(ownerFmt))) {
            StringCchCatW(msg, ARRAYSIZE(msg), ownerFmt);
        }
    }

    if (MessageBoxW(NULL, msg, NULL, MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2 | MB_SETFOREGROUND) != IDYES) {
        if (pOwnerSid) {
            LocalFree(pOwnerSid);
        }
        return;
    }
    if (pOwnerSid) {
        LocalFree(pOwnerSid);
    }

    EXPLICIT_ACCESSW ea = {0};
    ea.grfAccessPermissions = GENERIC_ALL;
    ea.grfAccessMode = GRANT_ACCESS;
    ea.grfInheritance = NO_INHERITANCE;
    BuildTrusteeWithSidW(&ea.Trustee, pSid);

    PACL pNewDacl = NULL;
    DWORD res = SetEntriesInAclW(1, &ea, NULL, &pNewDacl);
    if (res != ERROR_SUCCESS) {
        WCHAR errMsg[512] = {0};
        FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL, res, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       errMsg, ARRAYSIZE(errMsg), NULL);
        OutputDebugStringW(errMsg);
        MessageBoxRes(NULL, IDS_CREATE_DACL_FAILED, MB_ICONERROR, IDS_CAPTION_ERROR);
        return;
    }

    // PROTECTED_DACL_SECURITY_INFORMATION：切断继承（该按钮的既有语义）
    res = SetNamedSecurityInfoW((LPWSTR)filePath, SE_FILE_OBJECT,
                                DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                                NULL, NULL, pNewDacl, NULL);
    LocalFree(pNewDacl);

    BOOL ok = (res == ERROR_SUCCESS);
    MessageBoxRes(NULL, ok ? IDS_DACL_REPLACED : IDS_DACL_REPLACEMENT_FAILED,
                  ok ? MB_ICONINFORMATION : MB_ICONERROR,
                  ok ? IDS_CAPTION_SUCCESS : IDS_CAPTION_ERROR);
}

void ReplaceFullControlForMode(int mode, LPCWSTR filePath) {
    HANDLE hToken = NULL;
    if (!ImpersonateSystemToken(&hToken)) {
        return;
    }

    LPCWSTR sidStr = SidStringForMode(mode);
    PSID pSid = NULL;
    if (!ConvertStringSidToSidW(sidStr, &pSid)) {
        MessageBoxRes(NULL, (mode == MODE_SYSTEM) ? IDS_SID_SYSTEM_FAILED : IDS_SID_ADMIN_FAILED,
                      MB_ICONERROR, IDS_CAPTION_ERROR);
        RevertSystemToken(hToken);
        return;
    }

    ReplaceFullControlForSid(filePath, pSid, sidStr);
    LocalFree(pSid);
    RevertSystemToken(hToken);
}

void ForceDeleteFileNow(HWND hwnd, LPCWSTR filePath) {
    HANDLE hToken = NULL;
    if (!ImpersonateSystemToken(&hToken)) {
        MessageBoxRes(hwnd, IDS_DELETE_NO_TOKEN, MB_ICONERROR, IDS_CAPTION_ERROR);
        return;
    }

    if (DeleteFileW(filePath)) {
        MessageBoxRes(hwnd, IDS_DELETE_OK, MB_ICONINFORMATION, IDS_CAPTION_SUCCESS);
    } else {
        DWORD dwErr = GetLastError();
        MessageBoxResFmt(hwnd, IDS_DELETE_FAILED, MB_ICONERROR, IDS_CAPTION_ERROR, dwErr);
    }
    RevertSystemToken(hToken);
}

void DeleteFileOnReboot(HWND hwnd, LPCWSTR filePath) {
    HANDLE hToken = NULL;
    if (!ImpersonateSystemToken(&hToken)) {
        MessageBoxRes(hwnd, IDS_DELETE_NO_TOKEN, MB_ICONERROR, IDS_CAPTION_ERROR);
        return;
    }

    if (!EnableRestorePrivilege()) {
        MessageBoxRes(hwnd, IDS_REBOOT_PRIV_FAILED, MB_ICONERROR, IDS_CAPTION_ERROR);
        RevertSystemToken(hToken);
        return;
    }

    if (MoveFileExW(filePath, NULL, MOVEFILE_DELAY_UNTIL_REBOOT)) {
        MessageBoxRes(hwnd, IDS_REBOOT_OK, MB_ICONINFORMATION, IDS_CAPTION_SUCCESS);
    } else {
        DWORD dwErr = GetLastError();
        MessageBoxResFmt(hwnd, IDS_REBOOT_FAILED, MB_ICONERROR, IDS_CAPTION_ERROR, dwErr);
    }
    RevertSystemToken(hToken);
}
