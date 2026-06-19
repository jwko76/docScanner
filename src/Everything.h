// Everything.h - Everything SDK header
// From: https://www.voidtools.com/support/everything/sdk/
// Place Everything64.dll (or Everything32.dll) in the same folder as the executable.
//
// This header declares all exported functions.
// We load the DLL dynamically in everything_scanner.cpp.

#pragma once
#ifndef _INC_EVERYTHING
#define _INC_EVERYTHING

#include <windows.h>

// -------------------------
// Error codes
// -------------------------
#define EVERYTHING_OK                   0
#define EVERYTHING_ERROR_MEMORY         1   // out of memory
#define EVERYTHING_ERROR_IPC            2   // IPC not available (Everything not running)
#define EVERYTHING_ERROR_REGISTERCLASSEX 3
#define EVERYTHING_ERROR_CREATEWINDOW   4
#define EVERYTHING_ERROR_CREATETHREAD   5
#define EVERYTHING_ERROR_INVALIDINDEX   6
#define EVERYTHING_ERROR_INVALIDCALL    7

// -------------------------
// Request flags
// -------------------------
#define EVERYTHING_REQUEST_FILE_NAME                    0x00000001
#define EVERYTHING_REQUEST_PATH                         0x00000002
#define EVERYTHING_REQUEST_FULL_PATH_AND_FILE_NAME      0x00000004
#define EVERYTHING_REQUEST_EXTENSION                    0x00000008
#define EVERYTHING_REQUEST_SIZE                         0x00000010
#define EVERYTHING_REQUEST_DATE_CREATED                 0x00000020
#define EVERYTHING_REQUEST_DATE_MODIFIED                0x00000040
#define EVERYTHING_REQUEST_DATE_ACCESSED                0x00000080
#define EVERYTHING_REQUEST_ATTRIBUTES                   0x00000100
#define EVERYTHING_REQUEST_FILE_LIST_FILE_NAME          0x00000200
#define EVERYTHING_REQUEST_RUN_COUNT                    0x00000400
#define EVERYTHING_REQUEST_DATE_RUN                     0x00000800
#define EVERYTHING_REQUEST_DATE_RECENTLY_CHANGED        0x00001000
#define EVERYTHING_REQUEST_HIGHLIGHTED_FILE_NAME        0x00002000
#define EVERYTHING_REQUEST_HIGHLIGHTED_PATH             0x00004000
#define EVERYTHING_REQUEST_HIGHLIGHTED_FULL_PATH_AND_FILE_NAME 0x00008000

// -------------------------
// Sort types
// -------------------------
#define EVERYTHING_SORT_NAME_ASCENDING                  1
#define EVERYTHING_SORT_NAME_DESCENDING                 2
#define EVERYTHING_SORT_PATH_ASCENDING                  3
#define EVERYTHING_SORT_PATH_DESCENDING                 4
#define EVERYTHING_SORT_SIZE_ASCENDING                  5
#define EVERYTHING_SORT_SIZE_DESCENDING                 6
#define EVERYTHING_SORT_EXTENSION_ASCENDING             7
#define EVERYTHING_SORT_EXTENSION_DESCENDING            8
#define EVERYTHING_SORT_TYPE_NAME_ASCENDING             9
#define EVERYTHING_SORT_TYPE_NAME_DESCENDING            10
#define EVERYTHING_SORT_DATE_CREATED_ASCENDING          11
#define EVERYTHING_SORT_DATE_CREATED_DESCENDING         12
#define EVERYTHING_SORT_DATE_MODIFIED_ASCENDING         13
#define EVERYTHING_SORT_DATE_MODIFIED_DESCENDING        14
#define EVERYTHING_SORT_ATTRIBUTES_ASCENDING            15
#define EVERYTHING_SORT_ATTRIBUTES_DESCENDING           16
#define EVERYTHING_SORT_FILE_LIST_FILENAME_ASCENDING    17
#define EVERYTHING_SORT_FILE_LIST_FILENAME_DESCENDING   18
#define EVERYTHING_SORT_RUN_COUNT_ASCENDING             19
#define EVERYTHING_SORT_RUN_COUNT_DESCENDING            20
#define EVERYTHING_SORT_DATE_RECENTLY_CHANGED_ASCENDING 21
#define EVERYTHING_SORT_DATE_RECENTLY_CHANGED_DESCENDING 22
#define EVERYTHING_SORT_DATE_ACCESSED_ASCENDING         23
#define EVERYTHING_SORT_DATE_ACCESSED_DESCENDING        24
#define EVERYTHING_SORT_DATE_RUN_ASCENDING              25
#define EVERYTHING_SORT_DATE_RUN_DESCENDING             26

// -------------------------
// Function pointer types (Unicode versions)
// -------------------------
typedef void  (WINAPI *FnEverything_SetSearchW)(LPCWSTR lpString);
typedef void  (WINAPI *FnEverything_SetMatchPath)(BOOL bEnable);
typedef void  (WINAPI *FnEverything_SetMatchCase)(BOOL bEnable);
typedef void  (WINAPI *FnEverything_SetMatchWholeWord)(BOOL bEnable);
typedef void  (WINAPI *FnEverything_SetRegex)(BOOL bEnable);
typedef void  (WINAPI *FnEverything_SetMax)(DWORD dwMax);
typedef void  (WINAPI *FnEverything_SetOffset)(DWORD dwOffset);
typedef void  (WINAPI *FnEverything_SetReplyWindow)(HWND hWnd);
typedef void  (WINAPI *FnEverything_SetReplyID)(DWORD nId);
typedef void  (WINAPI *FnEverything_SetSort)(DWORD dwSortType);
typedef void  (WINAPI *FnEverything_SetRequestFlags)(DWORD dwRequestFlags);

typedef BOOL  (WINAPI *FnEverything_QueryW)(BOOL bWait);

typedef void  (WINAPI *FnEverything_SortResultsByPath)(void);
typedef void  (WINAPI *FnEverything_Reset)(void);
typedef void  (WINAPI *FnEverything_CleanUp)(void);

typedef DWORD (WINAPI *FnEverything_GetNumFileResults)(void);
typedef DWORD (WINAPI *FnEverything_GetNumFolderResults)(void);
typedef DWORD (WINAPI *FnEverything_GetNumResults)(void);
typedef DWORD (WINAPI *FnEverything_GetTotFileResults)(void);
typedef DWORD (WINAPI *FnEverything_GetTotFolderResults)(void);
typedef DWORD (WINAPI *FnEverything_GetTotResults)(void);
typedef BOOL  (WINAPI *FnEverything_IsVolumeResult)(DWORD nIndex);
typedef BOOL  (WINAPI *FnEverything_IsFolderResult)(DWORD nIndex);
typedef BOOL  (WINAPI *FnEverything_IsFileResult)(DWORD nIndex);

typedef void  (WINAPI *FnEverything_GetResultFileNameW)(DWORD nIndex, LPWSTR lpString, DWORD nMaxCount);
typedef void  (WINAPI *FnEverything_GetResultPathW)(DWORD nIndex, LPWSTR lpString, DWORD nMaxCount);
typedef void  (WINAPI *FnEverything_GetResultFullPathNameW)(DWORD nIndex, LPWSTR lpString, DWORD nMaxCount);
typedef BOOL  (WINAPI *FnEverything_GetResultSize)(DWORD nIndex, LARGE_INTEGER* lpFileSize);
typedef BOOL  (WINAPI *FnEverything_GetResultDateModified)(DWORD nIndex, FILETIME* lpDateModified);
typedef DWORD (WINAPI *FnEverything_GetLastError)(void);
typedef BOOL  (WINAPI *FnEverything_IsDBLoaded)(void);

typedef DWORD (WINAPI *FnEverything_GetMajorVersion)(void);
typedef DWORD (WINAPI *FnEverything_GetMinorVersion)(void);

#endif // _INC_EVERYTHING
