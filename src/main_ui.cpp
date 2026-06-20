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
#include <commctrl.h>
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

// ============================================================
// 사용자 정의 윈도우 메시지
// ============================================================
#define WM_SCAN_LOG       (WM_APP + 1)   // wParam = heap LPWSTR (수신 측에서 delete[])
#define WM_SCAN_PROGRESS  (WM_APP + 2)   // wParam = done, lParam = total
#define WM_SCAN_COMPLETE  (WM_APP + 3)   // wParam = piiFound, lParam = filesWithPii
#define WM_SCAN_FILES     (WM_APP + 4)   // wParam = totalFiles

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
};

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
static const int W  = 700;   // 클라이언트 폭 (approximate)
static const int M  = 12;    // 마진

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
    static HWND hHtmlBtn,  hExcelBtn;

    switch (msg) {

    // ── 창 생성 ───────────────────────────────────────────────
    case WM_CREATE: {
        INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_PROGRESS_CLASS };
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

        // 행3: 옵션
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

        // 행7: 로그창
        hLogEdit = CreateWindowW(L"EDIT", L"",
            WS_CHILD|WS_VISIBLE|WS_BORDER|WS_VSCROLL|
            ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL,
            M, y, W-24, 210, hwnd, (HMENU)IDC_LOG_EDIT, nullptr, nullptr);
        SendMessageW(hLogEdit, WM_SETFONT, (WPARAM)hF, TRUE);
        SendMessageW(hLogEdit, EM_LIMITTEXT, 0x100000, 0);
        y += 218;

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

    // 실제 클라이언트 크기 계산
    RECT rc = {0, 0, 700, 472};
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
