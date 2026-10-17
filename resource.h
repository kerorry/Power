#pragma once

// ---------- 资源 ID ----------
#define IDI_ICON1               100
#define IDD_TOOLBOX             1
#define IDC_TAB                 200

// ---------- 字符串：通用 ----------
#define IDS_APP_TITLE           101
#define IDS_ERR_GET_TOKEN       104
#define IDS_ERR_RUN             105
#define IDS_ERR_TASKDIALOG      109
#define IDS_ERR_UAC             110
#define IDS_ERR_LAUNCH          125
#define IDS_CAPTION_SUCCESS     111
#define IDS_CAPTION_ERROR       112

// ---------- 字符串：权限选择对话框（main.c） ----------
#define IDS_MENU_TITLE          127
#define IDS_MENU_CONTENT        128
#define IDS_MENU_BTN_ADMIN      129
#define IDS_MENU_BTN_SYSTEM     130
#define IDS_MENU_BTN_EXIT       131

// ---------- 字符串：标签页（ui.c） ----------
#define IDS_TAB1                102
#define IDS_TAB2                103

// ---------- 字符串：启动程序按钮文字 ----------
#define IDS_CMD_ARGS            216
#define IDS_CMD_TEXT            208
#define IDS_REGEDIT_TEXT        209
#define IDS_TASKMGR_TEXT        210
#define IDS_CALC_TEXT           211
#define IDS_CONTROL_TEXT        212
#define IDS_NOTEPAD_TEXT        213
#define IDS_EXPLORER_TEXT       214
#define IDS_MSCONFIG_TEXT       215

// ---------- 字符串：文件操作按钮文字 ----------
#define IDS_OWNER_ADMIN         400
#define IDS_OWNER_SYSTEM        401
#define IDS_ADD_PERM_ADMIN      402
#define IDS_ADD_PERM_SYSTEM     403
#define IDS_FORCE_DELETE        404
#define IDS_DELETE_ON_REBOOT    405

// ---------- 字符串：令牌与特权 ----------
#define IDS_SYSTEM_TOKEN_FAILED      114
#define IDS_IMPERSONATE_FAILED       115
#define IDS_TAKE_OWNERSHIP_FAILED    116
#define IDS_RESTORE_PRIVILEGE_FAILED 117
#define IDS_CONVERT_SID_FAILED       118

// ---------- 字符串：所有者与权限 ----------
#define IDS_OWNER_CHANGED            119
#define IDS_OWNER_CHANGE_FAILED      120
#define IDS_CREATE_DACL_FAILED       121
#define IDS_DACL_REPLACED            122
#define IDS_DACL_REPLACEMENT_FAILED  123
#define IDS_DACL_CONFIRM             124
#define IDS_DACL_REPLACE_TARGET      142
#define IDS_DACL_REMOVE_OWNER        143

// ---------- 字符串：文件删除 ----------
#define IDS_DELETE_NO_TOKEN          144
#define IDS_DELETE_OK                145
#define IDS_DELETE_FAILED            146
#define IDS_REBOOT_PRIV_FAILED       147
#define IDS_REBOOT_OK                148
#define IDS_REBOOT_FAILED            149
#define IDS_SID_ADMIN_FAILED         150
#define IDS_SID_SYSTEM_FAILED        151

// ---------- 控件 ID：启动程序页 ----------
#define IDC_CMD                 300
#define IDC_REGEDIT             301
#define IDC_TASKMGR             302
#define IDC_CALC                303
#define IDC_CONTROL             304
#define IDC_NOTEPAD             305
#define IDC_EXPLORER            306
#define IDC_MSCONFIG            307

// ---------- 控件 ID：文件操作页 ----------
#define IDC_OWNER_ADMIN         500
#define IDC_OWNER_SYSTEM        501
#define IDC_ADD_PERM_ADMIN      502
#define IDC_ADD_PERM_SYSTEM     503
#define IDC_FORCE_DELETE        504
#define IDC_DELETE_ON_REBOOT    505

// ---------- 权限选择对话框按钮 ID（TaskDialog 命令链接） ----------
#define IDM_MODE_ADMIN          1000
#define IDM_MODE_SYSTEM         1001
#define IDM_EXIT                1002
