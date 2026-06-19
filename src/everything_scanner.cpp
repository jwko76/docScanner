// everything_scanner.cpp
// Everything SDK IPC를 이용한 고속 파일 스캔

#include "everything_scanner.h"
#include "Everything.h"
#include <shlwapi.h>
#include <pathcch.h>
#include <algorithm>
#include <sstream>
#include <iostream>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "pathcch.lib")

// ============================================================
// 지원 확장자 목록 (정적)
// ============================================================

const std::set<std::wstring>& EverythingScanner::documentExtensions() {
    static const std::set<std::wstring> exts = {
        L".hwp", L".hwpx",
        L".doc",  L".docx",
        L".xls",  L".xlsx",
        L".ppt",  L".pptx",
        L".pdf",
        L".txt",  L".log",
        L".csv",  L".tsv",
        L".rtf",
        L".odt",  L".ods",  L".odp",
        L".xml",  L".json", L".html", L".htm",
        L".ini",  L".cfg",  L".conf",
    };
    return exts;
}

const std::set<std::wstring>& EverythingScanner::imageExtensions() {
    static const std::set<std::wstring> exts = {
        L".jpg",  L".jpeg",
        L".png",
        L".tif",  L".tiff",
        L".bmp",
        L".gif",
        L".webp",
    };
    return exts;
}

// ============================================================
// 생성자 / 소멸자
// ============================================================

EverythingScanner::EverythingScanner() = default;

EverythingScanner::~EverythingScanner() {
    if (m_CleanUp) {
        m_CleanUp();
    }
    if (m_hDll) {
        FreeLibrary(m_hDll);
        m_hDll = nullptr;
    }
}

// ============================================================
// 초기화
// ============================================================

bool EverythingScanner::initialize(const std::wstring& dllPath) {
    // DLL 경로 결정
    std::wstring path = dllPath;
    if (path.empty()) {
        // 실행 파일과 같은 폴더에서 탐색
        wchar_t exePath[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        PathRemoveFileSpecW(exePath);

        // 64비트 우선, 없으면 32비트
        std::wstring p64 = std::wstring(exePath) + L"\\Everything64.dll";
        std::wstring p32 = std::wstring(exePath) + L"\\Everything32.dll";
        if (GetFileAttributesW(p64.c_str()) != INVALID_FILE_ATTRIBUTES)
            path = p64;
        else if (GetFileAttributesW(p32.c_str()) != INVALID_FILE_ATTRIBUTES)
            path = p32;
        else {
            m_lastError = L"Everything64.dll / Everything32.dll을 찾을 수 없습니다. "
                          L"exe와 같은 폴더에 배치하세요.";
            return false;
        }
    }

    m_hDll = LoadLibraryW(path.c_str());
    if (!m_hDll) {
        m_lastError = L"DLL 로드 실패: " + path;
        return false;
    }

    if (!loadFunctions()) {
        FreeLibrary(m_hDll);
        m_hDll = nullptr;
        return false;
    }

    // Everything 실행 여부 확인
    if (m_IsDBLoaded && !m_IsDBLoaded()) {
        m_lastError = L"Everything이 실행 중이지 않거나 DB가 로드되지 않았습니다. "
                      L"Everything을 실행한 후 다시 시도하세요.";
        return false;
    }

    m_initialized = true;
    return true;
}

// ============================================================
// DLL 함수 포인터 로드
// ============================================================

#define LOAD_FN(var, name) \
    var = reinterpret_cast<decltype(var)>(GetProcAddress(m_hDll, name)); \
    if (!var) { m_lastError = L"함수를 찾을 수 없음: " #name; return false; }

bool EverythingScanner::loadFunctions() {
    LOAD_FN(m_SetSearch,       "Everything_SetSearchW");
    LOAD_FN(m_SetMatchPath,    "Everything_SetMatchPath");
    LOAD_FN(m_SetMatchCase,    "Everything_SetMatchCase");
    LOAD_FN(m_SetMax,          "Everything_SetMax");
    LOAD_FN(m_SetOffset,       "Everything_SetOffset");
    LOAD_FN(m_SetSort,         "Everything_SetSort");
    LOAD_FN(m_SetRequestFlags, "Everything_SetRequestFlags");
    LOAD_FN(m_Query,           "Everything_QueryW");
    LOAD_FN(m_GetNumResults,   "Everything_GetNumResults");
    LOAD_FN(m_GetTotResults,   "Everything_GetTotResults");
    LOAD_FN(m_IsFileResult,    "Everything_IsFileResult");
    LOAD_FN(m_GetFullPath,     "Everything_GetResultFullPathNameW");
    LOAD_FN(m_GetResultSize,   "Everything_GetResultSize");
    LOAD_FN(m_GetDateMod,      "Everything_GetResultDateModified");
    LOAD_FN(m_GetLastError,    "Everything_GetLastError");
    LOAD_FN(m_IsDBLoaded,      "Everything_IsDBLoaded");
    LOAD_FN(m_CleanUp,         "Everything_CleanUp");
    LOAD_FN(m_Reset,           "Everything_Reset");
    return true;
}

// ============================================================
// 검색 쿼리 생성
// 예) "C:\Users | ext:docx ext:pdf ..." → "path:C:\Users\ ext:docx|ext:pdf|..."
// Everything 검색 문법 사용
// ============================================================

std::wstring EverythingScanner::buildQuery(const std::wstring& rootPath) {
    // 문서 + 이미지 확장자를 ext: 형태로 조합
    std::wostringstream oss;

    // 경로 필터
    if (!rootPath.empty()) {
        oss << L"\"" << rootPath << L"\" ";
    }

    // 확장자 OR 조건
    oss << L"(";
    bool first = true;
    auto addExts = [&](const std::set<std::wstring>& exts) {
        for (const auto& e : exts) {
            // e는 ".docx" 형태, Everything은 "ext:docx" 형태 사용
            std::wstring ext = e.substr(1); // 점 제거
            if (!first) oss << L"|";
            oss << L"ext:" << ext;
            first = false;
        }
    };
    addExts(documentExtensions());
    addExts(imageExtensions());
    oss << L")";

    return oss.str();
}

// ============================================================
// 파일 스캔 실행
// ============================================================

std::vector<FileEntry> EverythingScanner::scanFiles(
    const std::wstring& rootPath,
    const ProgressCallback& progress)
{
    std::vector<FileEntry> results;
    if (!m_initialized) return results;

    // 쿼리 설정
    std::wstring query = buildQuery(rootPath);
    std::wcout << L"[Scanner] 검색 쿼리: " << query << L"\n";

    m_Reset();
    m_SetSearch(query.c_str());
    m_SetMatchCase(FALSE);
    m_SetMatchPath(rootPath.empty() ? FALSE : TRUE);
    m_SetSort(EVERYTHING_SORT_PATH_ASCENDING);
    m_SetRequestFlags(
        EVERYTHING_REQUEST_FULL_PATH_AND_FILE_NAME |
        EVERYTHING_REQUEST_SIZE |
        EVERYTHING_REQUEST_DATE_MODIFIED
    );
    // 최대 50만 건 (필요 시 조정)
    m_SetMax(500000);
    m_SetOffset(0);

    // 블로킹 쿼리 실행
    if (!m_Query(TRUE)) {
        DWORD err = m_GetLastError();
        m_lastError = L"Everything 쿼리 실패. 오류 코드: " + std::to_wstring(err);
        std::wcerr << L"[Scanner] " << m_lastError << L"\n";
        return results;
    }

    DWORD total = m_GetNumResults();
    std::wcout << L"[Scanner] 발견된 파일: " << total << L"개\n";

    const auto& docExts = documentExtensions();
    const auto& imgExts = imageExtensions();

    wchar_t pathBuf[32768] = {};

    for (DWORD i = 0; i < total; ++i) {
        if (!m_IsFileResult(i)) continue;

        // 전체 경로
        m_GetFullPath(i, pathBuf, (DWORD)std::size(pathBuf));
        std::wstring fullPath(pathBuf);

        // 확장자 추출 (소문자)
        std::wstring ext;
        size_t dotPos = fullPath.rfind(L'.');
        if (dotPos != std::wstring::npos) {
            ext = fullPath.substr(dotPos);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
        }

        // 파일 크기
        LARGE_INTEGER sz = {};
        LONGLONG fileSize = 0;
        if (m_GetResultSize(i, &sz)) {
            fileSize = sz.QuadPart;
        }

        // 수정 날짜
        FILETIME ft = {};
        m_GetDateMod(i, &ft);

        FileEntry entry;
        entry.fullPath    = fullPath;
        entry.extension   = ext;
        entry.fileSize    = fileSize;
        entry.dateModified = ft;
        entry.isDocument  = (docExts.count(ext) > 0);
        entry.isImage     = (imgExts.count(ext) > 0);

        results.push_back(std::move(entry));

        // 진행 상황 콜백 (1000건마다)
        if (progress && (i % 1000 == 0)) {
            progress(i, total);
        }
    }

    if (progress) progress(total, total);

    return results;
}

// ============================================================
// 내부 유틸
// ============================================================

std::wstring EverythingScanner::toLower(const std::wstring& s) {
    std::wstring out = s;
    std::transform(out.begin(), out.end(), out.begin(), ::towlower);
    return out;
}
