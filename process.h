// process.h
#pragma once
#include <windows.h>

DWORD FindPidByName(LPCWSTR name);
void RunProgram(int mode, LPCWSTR exePath, LPCWSTR args);
