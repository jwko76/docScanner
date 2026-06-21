// main_ui.cpp
// PiiScanner GUI - Win32 Windows Application
// 개인정보/민감정보 스캐너 GUI 버전
//
// 빌드: build_ui.bat
// 출력: build\PiiScannerUI.exe
//
// CLI 버전(PiiScanner.exe)과 동일한 스캔 엔진을 공유하며
// Win32 API 기반 창 UI 제공

// 빌드 시 /DNOMINMAX /DWIN32_LEAN_AND_MEAN /DUNICODE /D_UNICODE /D_WIN32_WINNT=0x0A00 전달됨
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <ctime>

#include "everything_scanner.h"
#include "text_extractor.h"
#include "pii_detector.h"
#include "reporter.h"

// ============================================================
// 컨트롤 ID
// ============================================================
#define IDC_SCAN_PATH_EDIT      101
#define IDC_SCAN_PATH_BROWSE    102
#define IDC_OUTPUT_PATH_EDIT    103
#define IDC_OUTPUT_PATH_BROWSE  104
#define IDC_SKIP_IMAGES_CHK     105
#define IDC_THREADS_EDIT        106
#define IDC_START_BTN           107
#define IDC_STOP_BTN            108
#define IDC_PROGRESS_BAR        109
#define IDC_STATUS_LABEL        110
#define IDC_LOG_EDIT            111
#define IDC_OPEN_HTML_BTN       112
#define IDC_OPEN_EXCEL_BTN      113
#define IDC_MAXSIZE_EDIT        114
#define IDC_TAB                 115
#define IDC_GRID                116
#define IDC_FILTER_COMBO        118   // 탐지 유형 필터 드롭다운
#define IDC_KEYWORD_EDIT        119   // 파일 키워드 검색 입력창
#define IDC_KEYWORD_FILE_BTN    120   // 키워드 파일 불러오기 버튼

// 우클릭 컨텍스트 메뉴 ID
#define IDM_OPEN_FILE   201
#define IDM_OPEN_FOLDER 202

// ============================================================
// 사용자 정의 윈도우 메시지
// ============================================================
#define WM_SCAN_LOG       (WM_APP + 1)   // wParam = heap LPWSTR (수신 측에서 delete[])
#define WM_SCAN_PROGRESS  (WM_APP + 2)   // wParam = done, lParam = total
#define WM_SCAN_COMPLETE  (WM_APP + 3)   // wParam = piiFound, lParam = filesWithPii
#define WM_SCAN_FILES     (WM_APP + 4)   // wParam = totalFiles
#define WM_SCAN_RESULT    (WM_APP + 5)   // wParam = heap ScanResultItem* (수신 측에서 delete)

// ============================================================
// 스캔 결과 아이템 (스레드 → UI 전달용)
// ============================================================
struct ScanResultItem {
    std::wstring filePath;    // 전체 경로
    std::wstring fileName;    // 파일명만
    std::wstring typeName;    // PII 유형
    std::wstring matchedText; // 탐지 원문
    std::wstring maskedText;  // 마스킹 값
    int          lineNumber = 0;
    std::wstring context;     // 맥락 (앞뒤 텍스트)
};

// ============================================================
// 파일 키워드 필터
// ============================================================
struct KeywordFilter {
    std::vector<std::wstring> orTerms;   // 스페이스 구분 OR 조건 (하나라도 일치)
    std::vector<std::wstring> andTerms;  // + 접두사 AND 조건 (모두 일치)
    std::vector<std::wstring> notTerms;  // - 접두사 NOT 조건 (모두 불일치)
    bool empty() const {
        return orTerms.empty() && andTerms.empty() && notTerms.empty();
    }
};

// ============================================================
// 전역 스캔 상태
// ============================================================
static HWND              g_hwnd = nullptr;
static std::atomic<bool> g_running{false};
static std::atomic<int>  g_done{0};
static std::atomic<int>  g_total{0};
static std::atomic<int>  g_piiFound{0};
static std::wstring      g_htmlPath;
static std::wstring      g_xlsxPath;
// 그리드 행별 전체 경로 (UI 스레드에서만 접근)
static std::vector<std::wstring> g_gridPaths;
// 전체 스캔 결과 사본 (정렬/필터 재구성용)
static std::vector<ScanResultItem> g_allItems;
// 그리드 정렬: sortCol -1=없음, sortDir 0=없음 1=오름차 -1=내림차
static int          g_sortCol = -1;
static int          g_sortDir = 0;
// 탐지 유형 필터 (L""=전체)
static std::wstring g_filterType;
// 핵심 컨트롤 핸들 (WndProc 외부 접근용)
static HWND         g_hGrid        = nullptr;
static HWND         g_hFilterCombo = nullptr;

// ============================================================
// 유틸
// ============================================================
static std::wstring CurrentTimeString() {
    auto t = std::time(nullptr);
    struct tm tm_info;
    localtime_s(&tm_info, &t);
    wchar_t buf[64];
    wcsftime(buf, std::size(buf), L"%Y-%m-%d %H:%M:%S", &tm_info);
    return buf;
}

static std::wstring CurrentTimestamp() {
    auto t = std::time(nullptr);
    struct tm tm_info;
    localtime_s(&tm_info, &t);
    wchar_t buf[32];
    wcsftime(buf, std::size(buf), L"%Y%m%d_%H%M%S", &tm_info);
    return buf;
}

static std::wstring GetCtrlText(HWND hCtrl) {
    int len = GetWindowTextLengthW(hCtrl);
    if (len <= 0) return L"";
    std::wstring s(len + 1, L'\0');
    GetWindowTextW(hCtrl, &s[0], len + 1);
    s.resize(len);
    return s;
}

static void AppendLog(HWND hLog, const std::wstring& msg) {
    int len = GetWindowTextLengthW(hLog);
    SendMessageW(hLog, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    std::wstring line = msg + L"\r\n";
    SendMessageW(hLog, EM_REPLACESEL, FALSE, (LPARAM)line.c_str());
}

static std::wstring BrowseFolder(HWND hwnd, const wchar_t* title) {
    wchar_t path[MAX_PATH] = {};
    BROWSEINFOW bi        = {};
    bi.hwndOwner          = hwnd;
    bi.lpszTitle          = title;
    bi.ulFlags            = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl     = SHBrowseForFolderW(&bi);
    if (pidl) {
        SHGetPathFromIDListW(pidl, path);
        CoTaskMemFree(pidl);
    }
    return path;
}

static void PostLog(HWND hwnd, const std::wstring& msg) {
    wchar_t* p = new wchar_t[msg.size() + 1];
    wcscpy_s(p, msg.size() + 1, msg.c_str());
    PostMessageW(hwnd, WM_SCAN_LOG, (WPARAM)p, 0);
}


// ============================================================
// 스캔 설정 구조체
// ============================================================
struct ScanConfig {
    std::wstring scanPath;
    std::wstring outputDir;
    bool         skipImages  = true;
    int          numThreads  = 0;        // 0 = 자동
    LONGLONG     maxFileSize = 100LL * 1024 * 1024;
    KeywordFilter keywordFilter;         // 파일 이름/경로 키워드 필터
};

// ============================================================
// 키워드 필터 헬퍼 (보안: 길이 제한, 입력 검증)
// ============================================================

// 유니코드 소문자 변환 (CharLowerBuffW - 한국어 포함 완전 지원)
static std::wstring ToLowerW(const std::wstring& s) {
    if (s.empty()) return s;
    std::wstring r = s;
    CharLowerBuffW(&r[0], (DWORD)r.size());
    return r;
}

// 키워드 한 줄 파싱: 스페이스=OR, +필수(AND), -배제(NOT), #주석
// 보안: 토큰 길이 256자 제한, 최대 100개 토큰
static KeywordFilter ParseKeywordLine(const std::wstring& line) {
    KeywordFilter kf;
    std::wistringstream iss(line);
    std::wstring token;
    int count = 0;
    while (iss >> token && count < 100) {
        if (token.size() > 256) token = token.substr(0, 256); // 길이 제한
        if (token[0] == L'#') break;                          // 주석 → 이후 무시
        else if (token[0] == L'+' && token.size() > 1) kf.andTerms.push_back(token.substr(1));
        else if (token[0] == L'-' && token.size() > 1) kf.notTerms.push_back(token.substr(1));
        else if (!token.empty())                        kf.orTerms.push_back(token);
        ++count;
    }
    return kf;
}

// 텍스트 파일에서 키워드 로드 (UTF-8 / ANSI 자동 감지)
// 보안: 파일 크기 64KB 제한, 500줄 제한
static KeywordFilter LoadKeywordsFromFile(const std::wstring& filePath) {
    KeywordFilter kf;
    if (filePath.empty() || filePath.size() > MAX_PATH) return kf;

    WIN32_FILE_ATTRIBUTE_DATA fad = {};
    if (!GetFileAttributesExW(filePath.c_str(), GetFileExInfoStandard, &fad)) return kf;
    ULONGLONG fileSize = ((ULONGLONG)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
    if (fileSize == 0 || fileSize > 65536) return kf;            // 64KB 제한

    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return kf;

    std::vector<char> buf((size_t)fileSize, 0);
    DWORD read = 0;
    bool ok = ReadFile(hFile, buf.data(), (DWORD)fileSize, &read, nullptr);
    CloseHandle(hFile);
    if (!ok || read == 0) return kf;

    // 널바이트 제거 (보안)
    for (auto& c : buf) if (c == '\0') c = ' ';

    // UTF-8 우선 시도, 실패 시 ANSI
    std::wstring content;
    int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        buf.data(), (int)read, nullptr, 0);
    if (wlen > 0) {
        content.resize(wlen);
        MultiByteToWideChar(CP_UTF8, 0, buf.data(), (int)read, &content[0], wlen);
    } else {
        wlen = MultiByteToWideChar(CP_ACP, 0, buf.data(), (int)read, nullptr, 0);
        if (wlen <= 0) return kf;
        content.resize(wlen);
        MultiByteToWideChar(CP_ACP, 0, buf.data(), (int)read, &content[0], wlen);
    }

    // 줄 단위 파싱 (최대 500줄)
    std::wistringstream iss(content);
    std::wstring line;
    int lineCount = 0;
    while (std::getline(iss, line) && lineCount < 500) {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        size_t s = line.find_first_not_of(L" \t");
        if (s == std::wstring::npos || line[s] == L'#') continue;
        line = line.substr(s);
        auto lineKf = ParseKeywordLine(line);
        for (auto& t : lineKf.orTerms)  kf.orTerms.push_back(t);
        for (auto& t : lineKf.andTerms) kf.andTerms.push_back(t);
        for (auto& t : lineKf.notTerms) kf.notTerms.push_back(t);
        ++lineCount;
    }
    return kf;
}

// 파일 경로가 키워드 필터 조건에 부합하는지 검사
// 평가 순서: NOT 우선 → AND → OR
static bool MatchesKeywordFilter(const std::wstring& filePath, const KeywordFilter& kf) {
    if (kf.empty()) return true;
    std::wstring lPath = ToLowerW(filePath);

    for (const auto& t : kf.notTerms)
        if (lPath.find(ToLowerW(t)) != std::wstring::npos) return false;

    for (const auto& t : kf.andTerms)
        if (lPath.find(ToLowerW(t)) == std::wstring::npos) return false;

    if (!kf.orTerms.empty()) {
        for (const auto& t : kf.orTerms)
            if (lPath.find(ToLowerW(t)) != std::wstring::npos) return true;
        return false;
    }
    return true;
}

// ============================================================
// 백그라운드 스캔 스레드
// ============================================================
static void ScanThread(HWND hwnd, ScanConfig cfg) {

    // Step 1: Everything SDK 초기화
    PostLog(hwnd, L"[1/4] Everything SDK 초기화 중...");
    EverythingScanner scanner;
    if (!scanner.initialize(L"")) {
        PostLog(hwnd, L"  ! Everything 미사용: " + scanner.getLastError());
        PostLog(hwnd, L"  → 파일시스템 직접 탐색 모드");
    } else {
        PostLog(hwnd, L"  ✓ Everything SDK 연결 성공");
    }
    if (!g_running) { PostLog(hwnd, L"[취소됨]"); PostMessageW(hwnd, WM_SCAN_COMPLETE, 0, 0); return; }

    // Step 2: 파일 목록 조회
    std::wstring pathDesc = cfg.scanPath.empty()
        ? L"전체 드라이브" : cfg.scanPath;
    PostLog(hwnd, L"[2/4] 파일 목록 조회 중 (" + pathDesc + L")...");

    auto tStart = std::chrono::steady_clock::now();

    std::vector<FileEntry> files = scanner.scanFiles(cfg.scanPath, nullptr);

    if (!g_running) { PostLog(hwnd, L"[취소됨]"); PostMessageW(hwnd, WM_SCAN_COMPLETE, 0, 0); return; }

    // 키워드 필터 적용 (파일 이름/경로 기준)
    if (!cfg.keywordFilter.empty()) {
        size_t before = files.size();
        files.erase(std::remove_if(files.begin(), files.end(),
            [&](const FileEntry& f) {
                return !MatchesKeywordFilter(f.fullPath, cfg.keywordFilter);
            }), files.end());
        PostLog(hwnd, L"  ✓ 키워드 필터: " + std::to_wstring(before)
            + L"개 → " + std::to_wstring(files.size()) + L"개 파일 선택");
    }

    int totalFiles = (int)files.size();
    g_total.store(totalFiles);
    PostMessageW(hwnd, WM_SCAN_FILES, (WPARAM)totalFiles, 0);

    int docCnt = 0, imgCnt = 0;
    for (const auto& f : files) {
        if (f.isDocument) ++docCnt;
        if (f.isImage)    ++imgCnt;
    }
    PostLog(hwnd, L"  ✓ 총 " + std::to_wstring(totalFiles) + L"개 파일 발견"
        + L" (문서 " + std::to_wstring(docCnt) + L"개, 이미지 " + std::to_wstring(imgCnt) + L"개)");

    if (files.empty()) {
        PostLog(hwnd, L"스캔할 파일이 없습니다.");
        PostMessageW(hwnd, WM_SCAN_COMPLETE, 0, 0);
        return;
    }

    // Step 3: PII 스캔 (멀티스레드)
    int nThreads = (cfg.numThreads > 0)
        ? cfg.numThreads
        : std::max(1, (int)std::thread::hardware_concurrency());
    PostLog(hwnd, L"[3/4] 개인정보 스캔 중 (스레드 " + std::to_wstring(nThreads) + L"개)...");

    std::vector<FileScanResult> results(files.size());
    std::atomic<int> nextIdx{0};
    std::atomic<int> done{0};

    auto worker = [&]() {
      try {
        TextExtractor extractor;
        extractor.setMaxTextLength(2'000'000);
        PiiDetector detector;

        while (g_running) {
            int idx = nextIdx.fetch_add(1);
            if (idx >= (int)files.size()) break;

            const auto& entry = files[idx];
            FileScanResult& res = results[idx];
            res.filePath  = entry.fullPath;
            res.extension = entry.extension;

            if (entry.fileSize > cfg.maxFileSize) {
                res.extractionSuccess = false;
                res.extractionError   = L"파일 크기 초과";
            } else if (cfg.skipImages && entry.isImage) {
                res.extractionSuccess = false;
                res.extractionError   = L"이미지 건너뜀";
            } else {
                ExtractionResult extracted = extractor.extract(entry.fullPath, entry.extension);
                res.extractionSuccess = extracted.success;
                res.extractionError   = extracted.errorMessage;
                res.extractionMethod  = extracted.method;
                res.textLength        = extracted.text.size();
                if (extracted.success && !extracted.text.empty()) {
                    res.matches = detector.detect(extracted.text);
                    g_piiFound.fetch_add((int)res.matches.size());

                    // 각 탐지 결과를 UI 그리드로 실시간 전송
                    for (const auto& m : res.matches) {
                        auto* ri = new ScanResultItem;
                        ri->filePath    = entry.fullPath;
                        auto sl = entry.fullPath.find_last_of(L"\\/");
                        ri->fileName    = (sl != std::wstring::npos)
                                          ? entry.fullPath.substr(sl + 1) : entry.fullPath;
                        ri->typeName    = m.typeName;
                        ri->matchedText = m.matchedText;
                        ri->maskedText  = m.maskedText;
                        ri->lineNumber  = m.lineNumber;
                        // 맥락: 제어문자 제거 + 최대 120자
                        std::wstring ctx;
                        for (wchar_t c : m.contextSnippet) {
                            if (c == L'\0') continue;
                            ctx += (c < 0x20) ? L' ' : c;
                        }
                        if (ctx.size() > 120) ctx = ctx.substr(0, 120) + L"…";
                        ri->context = std::move(ctx);
                        PostMessageW(hwnd, WM_SCAN_RESULT, (WPARAM)ri, 0);
                    }
                }
            }

            int d = ++done;
            g_done.store(d);
            if (d % 50 == 0 || d == totalFiles) {
                PostMessageW(hwnd, WM_SCAN_PROGRESS, (WPARAM)d, (LPARAM)totalFiles);
            }
        }
      } catch (const std::exception& ex) {
          std::string what = ex.what();
          PostLog(hwnd, L"[오류] 스레드 예외: " + std::wstring(what.begin(), what.end()));
      } catch (...) {
          PostLog(hwnd, L"[오류] 스레드 알 수 없는 예외");
      }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < nThreads; ++i) threads.emplace_back(worker);
    for (auto& t : threads) t.join();

    auto tEnd = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(tEnd - tStart).count();

    if (!g_running) {
        PostLog(hwnd, L"[스캔 중지됨]");
        PostMessageW(hwnd, WM_SCAN_COMPLETE, (WPARAM)g_piiFound.load(), 0);
        return;
    }

    PostLog(hwnd, L"  ✓ 스캔 완료 — 탐지 " + std::to_wstring(g_piiFound.load()) + L"건"
        + L", 소요 " + std::to_wstring((int)elapsed / 60) + L"분 "
        + std::to_wstring((int)elapsed % 60) + L"초");

    // Step 4: 리포트 생성
    PostLog(hwnd, L"[4/4] 리포트 생성 중...");

    ScanSummary summary;
    summary.scanPath          = pathDesc;
    summary.scanTime          = CurrentTimeString();
    summary.totalFilesScanned = totalFiles;
    summary.totalPiiFound     = g_piiFound.load();
    summary.totalScanSec      = elapsed;
    for (const auto& r : results) {
        if (r.totalMatches() > 0) ++summary.filesWithPii;
        for (const auto& m : r.matches) summary.piiTypeCounts[m.type]++;
    }

    CreateDirectoryW(cfg.outputDir.c_str(), nullptr);
    std::wstring baseName = L"pii_report_" + CurrentTimestamp();
    Reporter reporter;
    reporter.saveAll(results, summary, cfg.outputDir, baseName);

    g_htmlPath  = cfg.outputDir + L"\\" + baseName + L".html";
    g_xlsxPath  = cfg.outputDir + L"\\" + baseName + L".xlsx";

    PostLog(hwnd, L"  ✓ HTML:  " + g_htmlPath);
    PostLog(hwnd, L"  ✓ Excel: " + g_xlsxPath);
    PostLog(hwnd, L"─────────────────────────── 완료 ───────────────────────────");

    PostMessageW(hwnd, WM_SCAN_COMPLETE,
        (WPARAM)g_piiFound.load(), (LPARAM)summary.filesWithPii);
}


// ============================================================
// 레이아웃 상수
// ============================================================
static const int W  = 960;   // 클라이언트 폭
static const int M  = 12;    // 마진

// 그리드 컬럼 인덱스
enum GridCol { GC_FILE=0, GC_TYPE, GC_VALUE, GC_MASKED, GC_LINE, GC_CONTEXT, GC_COUNT };

// ============================================================
// 그리드 정렬 / 필터 헬퍼
// ============================================================

// 컬럼 기본 헤더 텍스트 (정렬 삼각형 추가 전 원본)
static const wchar_t* const k_colHeaders[GC_COUNT] = {
    L"파일명", L"탐지 유형", L"탐지 값", L"마스킹", L"줄", L"맥락"
};

static std::wstring GetItemField(const ScanResultItem& item, int col) {
    switch (col) {
    case GC_FILE:    return item.fileName;
    case GC_TYPE:    return item.typeName;
    case GC_VALUE:   return item.matchedText;
    case GC_MASKED:  return item.maskedText;
    case GC_LINE:    return std::to_wstring(item.lineNumber);
    case GC_CONTEXT: return item.context;
    default:         return L"";
    }
}

// 헤더 텍스트에 정렬 삼각형 직접 추가 (▲ 오름차 / ▼ 내림차)
// HDF_SORTUP/DOWN 은 ComCtl v6 매니페스트 없이는 표시 안 됨 → 텍스트 방식 사용
static void SetGridSortArrow(int sortCol, int sortDir) {
    if (!g_hGrid) return;
    HWND hHdr = ListView_GetHeader(g_hGrid);
    int n = Header_GetItemCount(hHdr);
    for (int i = 0; i < n && i < GC_COUNT; i++) {
        // 헤더 텍스트: 기본 이름 + 정렬 중인 컬럼에만 삼각형
        std::wstring txt = k_colHeaders[i];
        if (i == sortCol) {
            if (sortDir ==  1) txt += L" ▲";
            if (sortDir == -1) txt += L" ▼";
        }
        wchar_t buf[128];
        wcscpy_s(buf, std::size(buf), txt.c_str());

        HDITEMW hdi = {};
        hdi.mask     = HDI_TEXT | HDI_FORMAT;
        hdi.pszText  = buf;
        hdi.cchTextMax = (int)wcslen(buf);
        // 기존 format 읽어 정렬 방향 플래그도 함께 반영
        HDITEMW hdiFmt = {}; hdiFmt.mask = HDI_FORMAT;
        Header_GetItem(hHdr, i, &hdiFmt);
        hdiFmt.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
        if (i == sortCol) {
            if (sortDir ==  1) hdiFmt.fmt |= HDF_SORTUP;
            if (sortDir == -1) hdiFmt.fmt |= HDF_SORTDOWN;
        }
        hdi.fmt = hdiFmt.fmt;
        Header_SetItem(hHdr, i, &hdi);
    }
}

// 현재 필터 + 정렬 상태로 그리드 전체 재구성
static void RebuildGrid() {
    if (!g_hGrid) return;

    // 1. 필터 적용
    std::vector<size_t> idxs;
    idxs.reserve(g_allItems.size());
    for (size_t i = 0; i < g_allItems.size(); i++) {
        if (g_filterType.empty() || g_allItems[i].typeName == g_filterType)
            idxs.push_back(i);
    }

    // 2. 정렬
    if (g_sortCol >= 0 && g_sortDir != 0) {
        bool numCol = (g_sortCol == GC_LINE);
        std::stable_sort(idxs.begin(), idxs.end(), [&](size_t a, size_t b) {
            std::wstring va = GetItemField(g_allItems[a], g_sortCol);
            std::wstring vb = GetItemField(g_allItems[b], g_sortCol);
            int cmp;
            if (numCol) {
                int ia = _wtoi(va.c_str()), ib = _wtoi(vb.c_str());
                cmp = (ia < ib) ? -1 : (ia > ib) ? 1 : 0;
            } else {
                cmp = _wcsicmp(va.c_str(), vb.c_str());
            }
            return g_sortDir == 1 ? cmp < 0 : cmp > 0;
        });
    }

    // 3. ListView 재구성 (WM_SETREDRAW로 깜박임 방지)
    SendMessageW(g_hGrid, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(g_hGrid);
    g_gridPaths.clear();

    for (size_t idx : idxs) {
        const ScanResultItem& ri = g_allItems[idx];
        int row = ListView_GetItemCount(g_hGrid);
        g_gridPaths.push_back(ri.filePath);

        LVITEMW lvi = {};
        lvi.mask     = LVIF_TEXT;
        lvi.iItem    = row;
        lvi.iSubItem = GC_FILE;
        lvi.pszText  = const_cast<LPWSTR>(ri.fileName.c_str());
        ListView_InsertItem(g_hGrid, &lvi);

        std::wstring lineStr = std::to_wstring(ri.lineNumber);
        ListView_SetItemText(g_hGrid, row, GC_TYPE,    const_cast<LPWSTR>(ri.typeName.c_str()));
        ListView_SetItemText(g_hGrid, row, GC_VALUE,   const_cast<LPWSTR>(ri.matchedText.c_str()));
        ListView_SetItemText(g_hGrid, row, GC_MASKED,  const_cast<LPWSTR>(ri.maskedText.c_str()));
        ListView_SetItemText(g_hGrid, row, GC_LINE,    const_cast<LPWSTR>(lineStr.c_str()));
        ListView_SetItemText(g_hGrid, row, GC_CONTEXT, const_cast<LPWSTR>(ri.context.c_str()));
    }

    SendMessageW(g_hGrid, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_hGrid, nullptr, TRUE);
}

// ============================================================
// 윈도우 프로시저
// ============================================================
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // 컨트롤 핸들 (정적으로 유지)
    static HWND hScanEdit, hScanBrowse;
    static HWND hOutEdit,  hOutBrowse;
    static HWND hSkipChk,  hThreadsEdit, hMaxSizeEdit;
    static HWND hStartBtn, hStopBtn;
    static HWND hProgress;
    static HWND hStatus;
    static HWND hLogEdit;
    static HWND hTab, hGrid;
    static HWND hHtmlBtn,  hExcelBtn;
    static HWND hFilterCombo;
    static HWND hKeywordEdit, hKeywordFileBtn;

    switch (msg) {

    // ── 창 생성 ───────────────────────────────────────────────
    case WM_CREATE: {
        INITCOMMONCONTROLSEX icc = { sizeof(icc),
            ICC_PROGRESS_CLASS | ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES };
        InitCommonControlsEx(&icc);

        HFONT hF = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        int y = M;

        // 스캔 경로
        auto mkLabel = [&](const wchar_t* text, int x, int yt, int w, int ht) {
            HWND hc = CreateWindowW(L"STATIC", text, WS_CHILD|WS_VISIBLE, x, yt, w, ht, hwnd, nullptr, nullptr, nullptr);
            SendMessageW(hc, WM_SETFONT, (WPARAM)hF, TRUE);
        };
        auto mkEdit = [&](int id, int x, int yt, int w, int ht, DWORD extra = 0) {
            HWND hc = CreateWindowW(L"EDIT", L"",
                WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL|extra,
                x, yt, w, ht, hwnd, (HMENU)(INT_PTR)id, nullptr, nullptr);
            SendMessageW(hc, WM_SETFONT, (WPARAM)hF, TRUE);
            return hc;
        };
        auto mkBtn = [&](int id, const wchar_t* text, int x, int yt, int w, int ht, DWORD extra = 0) {
            HWND hc = CreateWindowW(L"BUTTON", text,
                WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|extra,
                x, yt, w, ht, hwnd, (HMENU)(INT_PTR)id, nullptr, nullptr);
            SendMessageW(hc, WM_SETFONT, (WPARAM)hF, TRUE);
            return hc;
        };

        // 행1: 스캔 경로
        mkLabel(L"스캔 경로:", M, y+3, 80, 20);
        hScanEdit   = mkEdit(IDC_SCAN_PATH_EDIT, 96, y, 492, 24);
        hScanBrowse = mkBtn(IDC_SCAN_PATH_BROWSE, L"찾아보기", 594, y, 82, 24);
        SendMessageW(hScanEdit, EM_SETCUEBANNER, FALSE, (LPARAM)L"(비워두면 전체 드라이브 탐색)");
        y += 34;

        // 행2: 출력 경로
        mkLabel(L"출력 경로:", M, y+3, 80, 20);
        hOutEdit   = mkEdit(IDC_OUTPUT_PATH_EDIT, 96, y, 492, 24);
        hOutBrowse = mkBtn(IDC_OUTPUT_PATH_BROWSE, L"찾아보기", 594, y, 82, 24);
        // 기본값: exe 폴더
        wchar_t exeDir[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exeDir, MAX_PATH);
        PathRemoveFileSpecW(exeDir);
        SetWindowTextW(hOutEdit, exeDir);
        y += 34;

        // 행3: 파일 키워드 검색
        mkLabel(L"키워드 검색:", M, y+3, 76, 20);
        hKeywordEdit = mkEdit(IDC_KEYWORD_EDIT, 92, y, 628, 24);
        SendMessageW(hKeywordEdit, EM_LIMITTEXT, 2048, 0);   // 2KB 제한 (보안)
        SendMessageW(hKeywordEdit, EM_SETCUEBANNER, FALSE,
            (LPARAM)L"스페이스=OR  +필수단어  -제외단어  확장자포함  예) 계약서 +2024 -임시 .xlsx");
        hKeywordFileBtn = mkBtn(IDC_KEYWORD_FILE_BTN, L"파일...", 726, y, 50, 24);
        y += 34;

        // 행4: 옵션
        hSkipChk = CreateWindowW(L"BUTTON", L"이미지 OCR 건너뜀 (빠른 스캔)",
            WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX, M, y+2, 220, 22,
            hwnd, (HMENU)IDC_SKIP_IMAGES_CHK, nullptr, nullptr);
        SendMessageW(hSkipChk, WM_SETFONT, (WPARAM)hF, TRUE);
        SendMessageW(hSkipChk, BM_SETCHECK, BST_CHECKED, 0);

        mkLabel(L"스레드:", 240, y+3, 55, 20);
        hThreadsEdit = mkEdit(IDC_THREADS_EDIT, 298, y, 45, 24, ES_NUMBER);
        SetWindowTextW(hThreadsEdit, L"0");
        mkLabel(L"(0=자동)", 348, y+3, 65, 20);

        mkLabel(L"최대 크기(MB):", 425, y+3, 95, 20);
        hMaxSizeEdit = mkEdit(IDC_MAXSIZE_EDIT, 524, y, 55, 24, ES_NUMBER);
        SetWindowTextW(hMaxSizeEdit, L"100");

        // 탐지 유형 필터 (행3 우측)
        mkLabel(L"탐지 유형:", 590, y+3, 74, 20);
        hFilterCombo = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL,
            666, y, 210, 220, hwnd, (HMENU)IDC_FILTER_COMBO, nullptr, nullptr);
        SendMessageW(hFilterCombo, WM_SETFONT, (WPARAM)hF, TRUE);
        ComboBox_AddString(hFilterCombo, L"전체 (필터 없음)");
        ComboBox_SetCurSel(hFilterCombo, 0);
        g_hFilterCombo = hFilterCombo;
        y += 32;

        // 행4: 버튼
        hStartBtn = mkBtn(IDC_START_BTN, L"▶  스캔 시작", M, y, 130, 32);
        hStopBtn  = mkBtn(IDC_STOP_BTN,  L"■  중 지",    150, y, 90, 32);
        EnableWindow(hStopBtn, FALSE);
        y += 42;

        // 행5: 진행률 바
        hProgress = CreateWindowW(PROGRESS_CLASSW, nullptr,
            WS_CHILD|WS_VISIBLE|PBS_SMOOTH,
            M, y, W-24, 18, hwnd, (HMENU)IDC_PROGRESS_BAR, nullptr, nullptr);
        SendMessageW(hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 1000));
        y += 26;

        // 행6: 상태
        hStatus = CreateWindowW(L"STATIC", L"준비",
            WS_CHILD|WS_VISIBLE|SS_LEFT, M, y, W-24, 20, hwnd, (HMENU)IDC_STATUS_LABEL, nullptr, nullptr);
        SendMessageW(hStatus, WM_SETFONT, (WPARAM)hF, TRUE);
        y += 28;

        // 행7: 탭컨트롤 (로그 / 스캔 결과)
        int tabH = 420;
        hTab = CreateWindowW(WC_TABCONTROLW, L"",
            WS_CHILD|WS_VISIBLE|TCS_FIXEDWIDTH,
            M, y, W-24, tabH, hwnd, (HMENU)IDC_TAB, nullptr, nullptr);
        SendMessageW(hTab, WM_SETFONT, (WPARAM)hF, TRUE);
        {
            TCITEMW ti = {}; ti.mask = TCIF_TEXT;
            ti.pszText = const_cast<LPWSTR>(L"로그");
            TabCtrl_InsertItem(hTab, 0, &ti);
            ti.pszText = const_cast<LPWSTR>(L"스캔 결과");
            TabCtrl_InsertItem(hTab, 1, &ti);
        }

        // 탭 내용 영역 계산
        RECT tabRC = { 0, 0, W-24, tabH };
        TabCtrl_AdjustRect(hTab, FALSE, &tabRC);
        int cx = M + tabRC.left, cy = y + tabRC.top;
        int cw = tabRC.right - tabRC.left, ch = tabRC.bottom - tabRC.top;

        // 탭0: 로그 EditBox
        hLogEdit = CreateWindowW(L"EDIT", L"",
            WS_CHILD|WS_VISIBLE|WS_VSCROLL|
            ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL,
            cx, cy, cw, ch, hwnd, (HMENU)IDC_LOG_EDIT, nullptr, nullptr);
        SendMessageW(hLogEdit, WM_SETFONT, (WPARAM)hF, TRUE);
        SendMessageW(hLogEdit, EM_LIMITTEXT, 0x200000, 0);

        // 탭1: ListView 결과 그리드 (초기 숨김)
        hGrid = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD|LVS_REPORT|LVS_SHOWSELALWAYS,
            cx, cy, cw, ch, hwnd, (HMENU)IDC_GRID, nullptr, nullptr);
        ListView_SetExtendedListViewStyle(hGrid,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
        g_hGrid = hGrid;

        // 그리드 컬럼 추가
        LVCOLUMNW lvc = {}; lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        auto addCol = [&](int sub, LPWSTR hdr, int w) {
            lvc.iSubItem = sub; lvc.pszText = hdr; lvc.cx = w;
            ListView_InsertColumn(hGrid, sub, &lvc);
        };
        addCol(GC_FILE,    const_cast<LPWSTR>(L"파일명"),   170);
        addCol(GC_TYPE,    const_cast<LPWSTR>(L"탐지 유형"), 90);
        addCol(GC_VALUE,   const_cast<LPWSTR>(L"탐지 값"),  130);
        addCol(GC_MASKED,  const_cast<LPWSTR>(L"마스킹"),   120);
        addCol(GC_LINE,    const_cast<LPWSTR>(L"줄"),        40);
        addCol(GC_CONTEXT, const_cast<LPWSTR>(L"맥락"),     cw-560);

        y += tabH + 8;

        // 행8: 리포트 열기 버튼
        hHtmlBtn  = mkBtn(IDC_OPEN_HTML_BTN,  L"HTML 리포트 열기",  M,       y, 160, 28);
        hExcelBtn = mkBtn(IDC_OPEN_EXCEL_BTN, L"Excel 리포트 열기", M+168,   y, 160, 28);
        EnableWindow(hHtmlBtn,  FALSE);
        EnableWindow(hExcelBtn, FALSE);

        return 0;
    }

    // ── 버튼 클릭 ────────────────────────────────────────────
    case WM_COMMAND: {
        WORD id = LOWORD(wParam);

        if (id == IDC_SCAN_PATH_BROWSE) {
            auto p = BrowseFolder(hwnd, L"스캔할 폴더 선택");
            if (!p.empty()) SetWindowTextW(hScanEdit, p.c_str());
        }
        else if (id == IDC_OUTPUT_PATH_BROWSE) {
            auto p = BrowseFolder(hwnd, L"리포트 저장 폴더 선택");
            if (!p.empty()) SetWindowTextW(hOutEdit, p.c_str());
        }
        else if (id == IDC_START_BTN) {
            ScanConfig cfg;
            cfg.scanPath   = GetCtrlText(hScanEdit);
            cfg.outputDir  = GetCtrlText(hOutEdit);
            cfg.skipImages = (SendMessageW(hSkipChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
            try { cfg.numThreads = std::stoi(GetCtrlText(hThreadsEdit)); } catch (...) { cfg.numThreads = 0; }
            try {
                long long mb = std::stoll(GetCtrlText(hMaxSizeEdit));
                if (mb > 0 && mb <= 10240) cfg.maxFileSize = mb * 1024 * 1024;
            } catch (...) {}

            // 키워드 필터 파싱 (직접 입력)
            std::wstring kwText = GetCtrlText(hKeywordEdit);
            if (!kwText.empty()) cfg.keywordFilter = ParseKeywordLine(kwText);

            // 출력 폴더 기본값
            if (cfg.outputDir.empty()) {
                wchar_t exeDir[MAX_PATH] = {};
                GetModuleFileNameW(nullptr, exeDir, MAX_PATH);
                PathRemoveFileSpecW(exeDir);
                cfg.outputDir = exeDir;
            }

            g_running.store(true);
            g_done.store(0); g_total.store(0); g_piiFound.store(0);
            g_htmlPath.clear(); g_xlsxPath.clear();

            // 그리드 초기화
            ListView_DeleteAllItems(hGrid);
            g_gridPaths.clear();
            g_allItems.clear();
            // 정렬 / 필터 초기화
            g_sortCol = -1; g_sortDir = 0;
            SetGridSortArrow(-1, 0);
            g_filterType.clear();
            if (hFilterCombo) {
                // 유형 목록 제거 (인덱스 0 "전체"만 유지)
                while (ComboBox_GetCount(hFilterCombo) > 1)
                    ComboBox_DeleteString(hFilterCombo, 1);
                ComboBox_SetCurSel(hFilterCombo, 0);
            }
            TabCtrl_SetCurSel(hTab, 0);  // 로그 탭으로 이동
            ShowWindow(hLogEdit, SW_SHOW);
            ShowWindow(hGrid,    SW_HIDE);

            SetWindowTextW(hLogEdit, L"");
            SendMessageW(hProgress, PBM_SETPOS, 0, 0);
            SetWindowTextW(hStatus, L"스캔 준비 중...");
            EnableWindow(hStartBtn,  FALSE);
            EnableWindow(hStopBtn,   TRUE);
            EnableWindow(hHtmlBtn,   FALSE);
            EnableWindow(hExcelBtn,  FALSE);

            std::thread([hwnd, cfg]() {
                ScanThread(hwnd, cfg);
                g_running.store(false);
            }).detach();
        }
        else if (id == IDC_STOP_BTN) {
            g_running.store(false);
            SetWindowTextW(hStatus, L"중지 요청 중... 현재 파일 처리 후 종료됩니다.");
            EnableWindow(hStopBtn, FALSE);
        }
        else if (id == IDC_OPEN_HTML_BTN && !g_htmlPath.empty()) {
            ShellExecuteW(hwnd, L"open", g_htmlPath.c_str(), nullptr, nullptr, SW_SHOW);
        }
        else if (id == IDC_OPEN_EXCEL_BTN && !g_xlsxPath.empty()) {
            ShellExecuteW(hwnd, L"open", g_xlsxPath.c_str(), nullptr, nullptr, SW_SHOW);
        }
        else if (id == IDC_KEYWORD_FILE_BTN) {
            // 키워드 텍스트 파일 선택 (보안: 사용자가 명시적으로 파일 선택)
            wchar_t filePath[MAX_PATH] = {};
            OPENFILENAMEW ofn       = {};
            ofn.lStructSize         = sizeof(ofn);
            ofn.hwndOwner           = hwnd;
            ofn.lpstrFilter         = L"텍스트 파일\0*.txt\0모든 파일\0*.*\0";
            ofn.lpstrFile           = filePath;
            ofn.nMaxFile            = MAX_PATH;
            ofn.lpstrTitle          = L"키워드 파일 선택";
            ofn.Flags               = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST
                                      | OFN_HIDEREADONLY;
            if (GetOpenFileNameW(&ofn)) {
                KeywordFilter kf = LoadKeywordsFromFile(filePath);
                // 로드한 키워드를 편집창에 표시 (OR 토큰 병합)
                std::wstring preview;
                for (auto& t : kf.andTerms) preview += L"+" + t + L" ";
                for (auto& t : kf.notTerms) preview += L"-" + t + L" ";
                for (auto& t : kf.orTerms)  preview += t + L" ";
                if (!preview.empty() && preview.back() == L' ')
                    preview.pop_back();
                SetWindowTextW(hKeywordEdit, preview.c_str());
            }
        }
        else if (id == IDC_FILTER_COMBO && HIWORD(wParam) == CBN_SELCHANGE) {
            int sel = ComboBox_GetCurSel(hFilterCombo);
            if (sel <= 0) {
                g_filterType.clear();      // 전체 (필터 없음)
            } else {
                wchar_t buf[128] = {};
                ComboBox_GetLBText(hFilterCombo, sel, buf);
                g_filterType = buf;
            }
            RebuildGrid();
        }
        return 0;
    }

    // ── 사용자 정의 메시지 ────────────────────────────────────
    case WM_SCAN_LOG: {
        wchar_t* msg = reinterpret_cast<wchar_t*>(wParam);
        if (msg) { AppendLog(hLogEdit, msg); delete[] msg; }
        return 0;
    }
    case WM_SCAN_FILES: {
        // 총 파일 수 수신 — 상태 레이블 업데이트
        SetWindowTextW(hStatus, (L"파일 " + std::to_wstring((int)wParam) + L"개 발견, 스캔 중...").c_str());
        return 0;
    }
    case WM_SCAN_PROGRESS: {
        int done  = (int)wParam;
        int total = (int)lParam;
        if (total > 0) {
            SendMessageW(hProgress, PBM_SETPOS, (WPARAM)(done * 1000LL / total), 0);
            std::wstring s = std::to_wstring(done) + L" / " + std::to_wstring(total)
                + L"  (탐지: " + std::to_wstring(g_piiFound.load()) + L"건)";
            SetWindowTextW(hStatus, s.c_str());
        }
        return 0;
    }
    case WM_SCAN_COMPLETE: {
        int pii       = (int)wParam;
        int filesWPii = (int)lParam;
        SendMessageW(hProgress, PBM_SETPOS, 1000, 0);
        std::wstring s = L"완료 — 파일 " + std::to_wstring(g_total.load())
            + L"개 / 개인정보 파일 " + std::to_wstring(filesWPii)
            + L"개 / 탐지 " + std::to_wstring(pii) + L"건";
        SetWindowTextW(hStatus, s.c_str());
        EnableWindow(hStartBtn,  TRUE);
        EnableWindow(hStopBtn,   FALSE);
        if (!g_htmlPath.empty())  EnableWindow(hHtmlBtn,  TRUE);
        if (!g_xlsxPath.empty())  EnableWindow(hExcelBtn, TRUE);
        // 탐지 결과가 있으면 스캔 결과 탭으로 자동 전환
        if (pii > 0) {
            TabCtrl_SetCurSel(hTab, 1);
            ShowWindow(hLogEdit, SW_HIDE);
            ShowWindow(hGrid,    SW_SHOW);
        }
        return 0;
    }

    // ── 스캔 결과 → 그리드 행 삽입 ──────────────────────────
    case WM_SCAN_RESULT: {
        auto* ri = reinterpret_cast<ScanResultItem*>(wParam);
        if (!ri) return 0;

        int row = ListView_GetItemCount(hGrid);
        g_gridPaths.push_back(ri->filePath);

        // 행 삽입
        LVITEMW lvi = {};
        lvi.mask    = LVIF_TEXT;
        lvi.iItem   = row;
        lvi.iSubItem = GC_FILE;
        lvi.pszText = const_cast<LPWSTR>(ri->fileName.c_str());
        ListView_InsertItem(hGrid, &lvi);

        // 서브아이템 채우기
        auto setSub = [&](int col, const std::wstring& txt) {
            ListView_SetItemText(hGrid, row, col, const_cast<LPWSTR>(txt.c_str()));
        };
        setSub(GC_TYPE,    ri->typeName);
        setSub(GC_VALUE,   ri->matchedText);
        setSub(GC_MASKED,  ri->maskedText);
        setSub(GC_LINE,    std::to_wstring(ri->lineNumber));
        setSub(GC_CONTEXT, ri->context);

        // 맨 아래로 스크롤
        ListView_EnsureVisible(hGrid, row, FALSE);

        // 전체 결과 사본 저장 (정렬/필터 재구성용)
        g_allItems.push_back(*ri);

        // 필터 콤보에 신규 탐지 유형 추가
        if (g_hFilterCombo) {
            int cnt = ComboBox_GetCount(g_hFilterCombo);
            bool found = false;
            for (int i = 1; i < cnt; i++) {
                wchar_t buf[128] = {};
                ComboBox_GetLBText(g_hFilterCombo, i, buf);
                if (ri->typeName == buf) { found = true; break; }
            }
            if (!found) ComboBox_AddString(g_hFilterCombo, ri->typeName.c_str());
        }

        delete ri;
        return 0;
    }

    // ── 탭 전환 / 그리드 더블클릭 알림 ──────────────────────
    case WM_NOTIFY: {
        auto* hdr = reinterpret_cast<NMHDR*>(lParam);

        // 탭 선택 변경
        if (hdr->hwndFrom == hTab && hdr->code == TCN_SELCHANGE) {
            int sel = TabCtrl_GetCurSel(hTab);
            ShowWindow(hLogEdit, sel == 0 ? SW_SHOW : SW_HIDE);
            ShowWindow(hGrid,    sel == 1 ? SW_SHOW : SW_HIDE);
        }

        // 컬럼 헤더 클릭 → 3단계 정렬 사이클 (오름차↑ → 내림차↓ → 없음)
        if (hdr->hwndFrom == hGrid && hdr->code == LVN_COLUMNCLICK) {
            auto* nmlv = reinterpret_cast<NMLISTVIEW*>(lParam);
            int col = nmlv->iSubItem;
            if (g_sortCol == col) {
                // 동일 컬럼: 1→-1→0 사이클
                if      (g_sortDir ==  1) g_sortDir = -1;
                else if (g_sortDir == -1) { g_sortDir = 0; g_sortCol = -1; }
            } else {
                g_sortCol = col;
                g_sortDir = 1;
            }
            SetGridSortArrow(g_sortCol, g_sortDir);
            RebuildGrid();
        }

        // ListView 더블클릭 → 파일 열기 + 탐색기에서 폴더 열기
        if (hdr->hwndFrom == hGrid && hdr->code == NM_DBLCLK) {
            int sel = ListView_GetNextItem(hGrid, -1, LVNI_SELECTED);
            if (sel >= 0 && sel < (int)g_gridPaths.size()) {
                const std::wstring& path = g_gridPaths[sel];
                // 1) 소스 파일 직접 열기
                ShellExecuteW(hwnd, L"open", path.c_str(), nullptr, nullptr, SW_SHOW);
                // 2) 탐색기에서 해당 파일을 선택 상태로 폴더 열기
                std::wstring arg = L"/select,\"" + path + L"\"";
                ShellExecuteW(hwnd, L"open", L"explorer.exe",
                    arg.c_str(), nullptr, SW_SHOW);
            }
        }
        return 0;
    }

    // ── 그리드 우클릭 컨텍스트 메뉴 ─────────────────────────
    case WM_CONTEXTMENU: {
        if ((HWND)wParam != hGrid) break;
        int sel = ListView_GetNextItem(hGrid, -1, LVNI_SELECTED);
        if (sel < 0 || sel >= (int)g_gridPaths.size()) return 0;

        HMENU hMenu = CreatePopupMenu();
        AppendMenuW(hMenu, MF_STRING, IDM_OPEN_FILE,   L"파일 열기(&O)");
        AppendMenuW(hMenu, MF_STRING, IDM_OPEN_FOLDER, L"폴더 열기(&F)");

        int cmd = (int)TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
            LOWORD(lParam), HIWORD(lParam), 0, hwnd, nullptr);
        DestroyMenu(hMenu);

        const std::wstring& path = g_gridPaths[sel];
        if (cmd == IDM_OPEN_FILE) {
            ShellExecuteW(hwnd, L"open", path.c_str(), nullptr, nullptr, SW_SHOW);
        } else if (cmd == IDM_OPEN_FOLDER) {
            // 탐색기에서 파일 선택 표시
            std::wstring arg = L"/select,\"" + path + L"\"";
            ShellExecuteW(hwnd, L"open", L"explorer.exe",
                arg.c_str(), nullptr, SW_SHOW);
        }
        return 0;
    }

    case WM_DESTROY:
        g_running.store(false);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ============================================================
// wWinMain
// ============================================================
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nCmdShow) {
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    WNDCLASSEXW wc   = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"PiiScannerUI";
    wc.hIcon         = LoadIconW(nullptr, IDI_SHIELD);
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    // 실제 클라이언트 크기 계산 (960 × 694, 키워드 행 34px 추가)
    RECT rc = {0, 0, 960, 694};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, FALSE);

    g_hwnd = CreateWindowExW(
        0, L"PiiScannerUI",
        L"PiiScanner UI — 개인정보/민감정보 탐지",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInst, nullptr
    );

    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CoUninitialize();
    return (int)msg.wParam;
}
