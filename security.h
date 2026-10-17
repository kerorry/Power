// security.h
#pragma once

#include <windows.h>

BOOL IsAdmin(void);
BOOL IsSystem(void);
BOOL EnableSystemTokenPrivileges(void);
HANDLE GetSystemToken(void);
BOOL EnableRestorePrivilege(void);
BOOL EnableTakeOwnershipPrivilege(void);
