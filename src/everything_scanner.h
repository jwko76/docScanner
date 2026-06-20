#pragma once
// everything_scanner.h
// Everything SDK를 이용해 파일을 빠르게 검색하는 모듈

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <set>
#include "Everything.h"  // FnEverything_* 함수 포인터 타입 정의

// -------------------------
// 파일 항목 구조체
// -------------------------
struct FileEntry {
    std::wstring fullPath;
    std::wstring extension;     // 소문자 확장자 (예: L".docx")
    LONGLONG     fileSize;      // 바이트
    FILETIME     dateModified;
    bool         isDocument;    // 문서 파일 여부
    bool         isImage;       // 이미지 파일 여부
};

// -------------------------
// 스캔 진행 콜백: (완료 건수, 전체 건수)
// -------------------------
using ProgressCallback = std::function<void(DWORD current, DWORD total)>;

// -------------------------
// EverythingScanner 클래스
// Everything SDK DLL을 동적으로 로딩하여 IPC 쿼리 실행
// Everything 프로그램이 백그라운드에서 실행 중이어야 함
// -------------------------
class EverythingScanner {
public:
    EverythingScanner();
    ~EverythingScanner();

    // 초기화 (DLL 로드 및 Everything 연결 확인)
    // dllPath: Everything64.dll 또는 Everything32.dll 경로 (비어 있으면 exe 폴더에서 탐색)
    bool initialize(const std::wstring& dllPath = L"");

    bool isAvailable() const { return m_initialized; }
    std::wstring getLastError() const { return m_lastError; }

    // 문서 + 이미지 파일을 Everything으로 검색하여 반환
    // rootPath: 비어 있으면 전체 드라이브, 지정하면 해당 경로 이하만 검색
    std::vector<FileEntry> scanFiles(
        const std::wstring& rootPath = L"",
        const ProgressCallback& progress = nullptr
    );

    // 지원 확장자 목록 (정적)
    static const std::set<std::wstring>& documentExtensions();
    static const std::set<std::wstring>& imageExtensions();

private:
    bool         m_initialized = false;
    std::wstring m_lastError;
    HMODULE      m_hDll = nullptr;

    // ---- 함수 포인터 ----
    FnEverything_SetSearchW        m_SetSearch       = nullptr;
    FnEverything_SetMatchPath      m_SetMatchPath    = nullptr;
    FnEverything_SetMatchCase      m_SetMatchCase    = nullptr;
    FnEverything_SetMax            m_SetMax          = nullptr;
    FnEverything_SetOffset         m_SetOffset       = nullptr;
    FnEverything_SetSort           m_SetSort         = nullptr;
    FnEverything_SetRequestFlags   m_SetRequestFlags = nullptr;
    FnEverything_QueryW            m_Query           = nullptr;
    FnEverything_GetNumResults     m_GetNumResults   = nullptr;
    FnEverything_GetTotResults     m_GetTotResults   = nullptr;
    FnEverything_IsFileResult      m_IsFileResult    = nullptr;
    FnEverything_GetResultFullPathNameW m_GetFullPath = nullptr;
    FnEverything_GetResultSize     m_GetResultSize   = nullptr;
    FnEverything_GetResultDateModified m_GetDateMod  = nullptr;
    FnEverything_GetLastError      m_GetLastError    = nullptr;
    FnEverything_IsDBLoaded        m_IsDBLoaded      = nullptr;
    FnEverything_CleanUp           m_CleanUp         = nullptr;
    FnEverything_Reset             m_Reset           = nullptr;

    bool loadFunctions();
    std::wstring buildQuery(const std::wstring& rootPath);
    std::wstring toLower(const std::wstring& s);

    // Everything 없이 FindFirstFileW 로 직접 디렉터리를 재귀 탐색하는 폴백
    // rootPath가 비어 있으면 전체 드라이브를 탐색
    std::vector<FileEntry> scanFilesFs(
        const std::wstring& rootPath,
        const ProgressCallback& progress
    );
};
