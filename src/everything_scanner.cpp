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

    // [보안] 사용자 공급 DLL 경로 검증:
    //   1) 파일이 실제 존재하는지 확인
    //   2) 확장자가 .dll인지 확인 (대소문자 무시)
    //   → LoadLibraryW 전에 검증하지 않으면 임의 DLL 로드 가능 (DLL 인젝션)
    {
        DWORD attr = GetFileAttributesW(path.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) {
            m_lastError = L"DLL 파일을 찾을 수 없습니다: " + path;
            return false;
        }
        // 확장자 검사 (.dll만 허용)
        auto dotPos = path.rfind(L'.');
        if (dotPos == std::wstring::npos) {
            m_lastError = L"DLL 경로의 확장자가 없습니다: " + path;
            return false;
        }
        std::wstring ext = path.substr(dotPos);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
        if (ext != L".dll") {
            m_lastError = L"DLL 경로는 .dll 확장자여야 합니다: " + path;
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

    // [보안] 경로 필터: 큰따옴표를 제거하여 쿼리 인젝션 방지
    //   Everything 검색 문법에서 "path" 형태로 경로를 감싸는데,
    //   rootPath에 큰따옴표가 포함되면 쿼리 구조가 깨짐
    if (!rootPath.empty()) {
        std::wstring safePath = rootPath;
        safePath.erase(std::remove(safePath.begin(), safePath.end(), L'"'), safePath.end());
        safePath.erase(std::remove(safePath.begin(), safePath.end(), L'|'), safePath.end());
        oss << L"\"" << safePath << L"\" ";
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

    // Everything을 사용할 수 없으면 파일시스템 직접 탐색으로 폴백
    if (!m_initialized) {
        std::wcout << L"[Scanner] Everything 미사용 → 파일시스템 직접 탐색 모드\n";
        return scanFilesFs(rootPath, progress);
    }

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
        std::wcout << L"[Scanner] → 파일시스템 직접 탐색으로 폴백\n";
        return scanFilesFs(rootPath, progress);
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

// ============================================================
// 파일시스템 직접 탐색 폴백 (FindFirstFileW 재귀)
// Everything 없이도 동작하도록 하는 대체 스캔 방법
// ============================================================

std::vector<FileEntry> EverythingScanner::scanFilesFs(
    const std::wstring& rootPath,
    const ProgressCallback& progress)
{
    std::vector<FileEntry> results;
    const auto& docExts = documentExtensions();
    const auto& imgExts = imageExtensions();

    // 재귀 탐색 람다 (std::function 사용)
    std::function<void(const std::wstring&)> walk =
        [&](const std::wstring& dir) {

        WIN32_FIND_DATAW ffd = {};
        std::wstring pattern = dir + L"\\*";
        HANDLE hFind = FindFirstFileW(pattern.c_str(), &ffd);
        if (hFind == INVALID_HANDLE_VALUE) return;

        do {
            const std::wstring name = ffd.cFileName;
            // 현재 폴더(.) 와 부모 폴더(..) 건너뜀
            if (name == L"." || name == L"..") continue;

            std::wstring fullPath = dir + L"\\" + name;

            if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                // [보안] 심볼릭 링크/정션(재파스 포인트) 건너뜀 → 무한 루프 방지
                if (ffd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
                walk(fullPath);
            } else {
                // 확장자 추출 (소문자)
                std::wstring ext;
                size_t dotPos = name.rfind(L'.');
                if (dotPos != std::wstring::npos) {
                    ext = name.substr(dotPos);
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
                }

                bool isDoc = (docExts.count(ext) > 0);
                bool isImg = (imgExts.count(ext) > 0);
                if (!isDoc && !isImg) continue;  // 관심 확장자 외 건너뜀

                LARGE_INTEGER fileSize;
                fileSize.HighPart = ffd.nFileSizeHigh;
                fileSize.LowPart  = ffd.nFileSizeLow;

                FileEntry entry;
                entry.fullPath     = fullPath;
                entry.extension    = ext;
                entry.fileSize     = fileSize.QuadPart;
                entry.dateModified = ffd.ftLastWriteTime;
                entry.isDocument   = isDoc;
                entry.isImage      = isImg;
                results.push_back(std::move(entry));

                // 1000건마다 진행률 콜백 (전체 건수는 미리 알 수 없어 0으로 전달)
                if (progress && (results.size() % 1000 == 0)) {
                    progress((DWORD)results.size(), 0);
                }
            }
        } while (FindNextFileW(hFind, &ffd));

        FindClose(hFind);
    };

    if (rootPath.empty()) {
        // 경로 미지정 → 전체 드라이브 탐색
        DWORD drives = GetLogicalDrives();
        for (int i = 0; i < 26; ++i) {
            if (!(drives & (1u << i))) continue;
            wchar_t driveRoot[4] = {
                (wchar_t)(L'A' + i), L':', L'\\', L'\0'
            };
            std::wcout << L"  [FS] 드라이브 탐색: " << driveRoot << L"\n";
            walk(driveRoot);
        }
    } else {
        walk(rootPath);
    }

    if (progress) progress((DWORD)results.size(), (DWORD)results.size());

    return results;
}
