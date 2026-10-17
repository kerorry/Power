// fileops.h
#pragma once

#include <windows.h>

// mode 取 MODE_ADMIN 或 MODE_SYSTEM
void ChangeOwnerForMode(int mode, LPCWSTR filePath);
void ReplaceFullControlForMode(int mode, LPCWSTR filePath);

void ForceDeleteFileNow(HWND hwnd, LPCWSTR filePath);
void DeleteFileOnReboot(HWND hwnd, LPCWSTR filePath);
