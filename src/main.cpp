// main.cpp
// PiiScanner - 개인정보/민감정보 파일 스캐너
//
// 사용법:
//   PiiScanner.exe [옵션]
//   --path <경로>    : 스캔 경로 (기본: 전체 드라이브)
//   --output <경로>  : 리포트 저장 경로 (기본: 실행 폴더)
//   --skip-images    : 이미지 OCR 건너뜀 (빠른 스캔)
//   --max-size <MB>  : 파일 최대 크기 (기본: 100MB)
//   --threads <N>    : 병렬 스캔 스레드 수 (기본: CPU 코어 수)
//
// 의존성:
//   - Everything 실행 중 필요 + Everything64.dll 동일 폴더
//   - Windows 10+ (Windows OCR API)
//   - Microsoft Office 설치 시 IFilter 자동 지원

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0A00

#include <windows.h>
#include <shlwapi.h>   // PathRemoveFileSpecW
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <ctime>

#include "everything_scanner.h"
#include "text_extractor.h"
#include "pii_detector.h"
#include "reporter.h"

// ============================================================
// 전역 설정
// ============================================================

struct AppConfig {
    std::wstring scanPath;          // 빈 문자열 = 전체 드라이브
    std::wstring outputDir;         // 리포트 저장 폴더
    bool         skipImages  = false;
    LONGLONG     maxFileSize = 100LL * 1024 * 1024; // 100 MB
    int          numThreads  = -1;  // -1 = auto (CPU 코어 수)
    std::wstring dllPath;           // Everything DLL 경로 (선택)
};

// ============================================================
// 유틸
// ============================================================

static std::wstring currentTimeString() {
    auto t = std::time(nullptr);
    struct tm tm_info;
    localtime_s(&tm_info, &t);
    wchar_t buf[64];
    wcsftime(buf, std::size(buf), L"%Y-%m-%d %H:%M:%S", &tm_info);
    return buf;
}

static std::wstring currentTimestamp() {
    auto t = std::time(nullptr);
    struct tm tm_info;
    localtime_s(&tm_info, &t);
    wchar_t buf[32];
    wcsftime(buf, std::size(buf), L"%Y%m%d_%H%M%S", &tm_info);
    return buf;
}

static void printProgress(int done, int total, int piiFound) {
    if (total == 0) return;
    int pct = done * 100 / total;
    int barW = 40;
    int fill  = pct * barW / 100;
    std::wcout << L"\r[";
    for (int i = 0; i < fill; ++i)  std::wcout << L"=";
    for (int i = fill; i < barW; ++i) std::wcout << L" ";
    std::wcout << L"] " << std::setw(3) << pct << L"% "
               << done << L"/" << total
               << L" (탐지: " << piiFound << L"건)   ";
    std::wcout.flush();
}

// ============================================================
// 인자 파싱
// ============================================================

static AppConfig parseArgs(int argc, wchar_t* argv[]) {
    AppConfig cfg;

    // 기본 출력 폴더: 실행 파일 위치
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    PathRemoveFileSpecW(exePath);
    cfg.outputDir = exePath;

    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if      (arg == L"--path"        && i+1 < argc) cfg.scanPath    = argv[++i];
        else if (arg == L"--output"      && i+1 < argc) cfg.outputDir   = argv[++i];
        else if (arg == L"--dll"         && i+1 < argc) cfg.dllPath     = argv[++i];
        else if (arg == L"--skip-images")               cfg.skipImages  = true;
        else if (arg == L"--max-size"    && i+1 < argc) {
            try {
                long long mb = std::stoll(argv[++i]);
                if (mb <= 0 || mb > 10240) { // 1~10240 MB
                    std::wcerr << L"[오류] --max-size 값은 1~10240(MB) 범위여야 합니다.\n";
                    exit(1);
                }
                cfg.maxFileSize = mb * 1024 * 1024;
            } catch (const std::exception&) {
                std::wcerr << L"[오류] --max-size 인자가 숫자가 아닙니다: " << argv[i] << L"\n";
                exit(1);
            }
        }
        else if (arg == L"--threads"     && i+1 < argc) {
            try {
                int n = std::stoi(argv[++i]);
                if (n <= 0 || n > 256) {
                    std::wcerr << L"[오류] --threads 값은 1~256 범위여야 합니다.\n";
                    exit(1);
                }
                cfg.numThreads = n;
            } catch (const std::exception&) {
                std::wcerr << L"[오류] --threads 인자가 숫자가 아닙니다: " << argv[i] << L"\n";
                exit(1);
            }
        }
        else if (arg == L"--help" || arg == L"-h") {
            std::wcout <<
                L"사용법: PiiScanner.exe [옵션]\n"
                L"  --path <경로>      스캔 경로 (기본: 전체 드라이브)\n"
                L"  --output <경로>    리포트 저장 폴더 (기본: 실행 파일 폴더)\n"
                L"  --dll <경로>       Everything64.dll 경로 지정\n"
                L"  --skip-images      이미지 OCR 건너뜀\n"
                L"  --max-size <MB>    최대 파일 크기 MB (기본 100)\n"
                L"  --threads <N>      병렬 스레드 수 (기본 자동)\n";
            exit(0);
        }
    }
    return cfg;
}

// ============================================================
// 메인 스캔 루프 (멀티스레드)
// ============================================================

static std::vector<FileScanResult> scanAll(
    const std::vector<FileEntry>& files,
    const AppConfig& cfg,
    std::atomic<int>& piiFound)
{
    int nThreads = (cfg.numThreads > 0)
        ? cfg.numThreads
        : std::max(1, (int)std::thread::hardware_concurrency());

    std::vector<FileScanResult> results(files.size());
    std::atomic<int> nextIdx{0};
    std::atomic<int> done{0};
    std::mutex printMutex;

    auto worker = [&]() {
        // 각 스레드는 자체 TextExtractor와 PiiDetector 인스턴스 사용
        TextExtractor extractor;
        extractor.setMaxTextLength(2'000'000);  // 최대 2MB 텍스트

        PiiDetector detector;

        while (true) {
            int idx = nextIdx.fetch_add(1);
            if (idx >= (int)files.size()) break;

            const auto& entry = files[idx];
            FileScanResult& res = results[idx];

            res.filePath  = entry.fullPath;
            res.extension = entry.extension;

            // 파일 크기 초과 건너뜀
            if (entry.fileSize > cfg.maxFileSize) {
                res.extractionSuccess = false;
                res.extractionError   = L"파일 크기 초과 (" +
                    std::to_wstring(entry.fileSize / 1024 / 1024) + L"MB)";
                ++done;
                continue;
            }

            // 이미지 건너뜀 설정
            if (cfg.skipImages && entry.isImage) {
                res.extractionSuccess = false;
                res.extractionError   = L"이미지 스캔 건너뜀 (--skip-images)";
                ++done;
                continue;
            }

            auto t0 = std::chrono::steady_clock::now();

            // 텍스트 추출
            ExtractionResult extracted = extractor.extract(
                entry.fullPath, entry.extension
            );

            res.extractionSuccess = extracted.success;
            res.extractionError   = extracted.errorMessage;
            res.extractionMethod  = extracted.method;
            res.textLength        = extracted.text.size();

            // PII 탐지
            if (extracted.success && !extracted.text.empty()) {
                res.matches = detector.detect(extracted.text);
                piiFound.fetch_add((int)res.matches.size());
            }

            auto t1 = std::chrono::steady_clock::now();
            res.scanTimeSec = std::chrono::duration<double>(t1 - t0).count();

            int doneCount = ++done;

            // 진행률 출력 (100건마다)
            if (doneCount % 100 == 0) {
                std::lock_guard<std::mutex> lock(printMutex);
                printProgress(doneCount, (int)files.size(), piiFound.load());
            }
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(nThreads);
    for (int i = 0; i < nThreads; ++i)
        threads.emplace_back(worker);
    for (auto& t : threads) t.join();

    // 마지막 진행률 출력
    printProgress((int)files.size(), (int)files.size(), piiFound.load());
    std::wcout << L"\n";

    return results;
}

// ============================================================
// wmain
// ============================================================

int wmain(int argc, wchar_t* argv[]) {
    // 콘솔 출력 UTF-8 설정
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    std::wcout << L"====================================================\n";
    std::wcout << L"  PiiScanner - 개인정보/민감정보 스캐너 v1.0\n";
    std::wcout << L"====================================================\n\n";

    AppConfig cfg = parseArgs(argc, argv);

    // ---- Step 1: Everything SDK 초기화 ----
    std::wcout << L"[1/4] Everything SDK 초기화 중...\n";
    EverythingScanner scanner;
    if (!scanner.initialize(cfg.dllPath)) {
        std::wcerr << L"[오류] " << scanner.getLastError() << L"\n";
        std::wcerr << L"  → Everything(https://www.voidtools.com/)을 먼저 실행하세요.\n";
        return 1;
    }
    std::wcout << L"  ✓ Everything SDK 연결 성공\n\n";

    // ---- Step 2: 파일 목록 조회 ----
    std::wcout << L"[2/4] 파일 목록 조회 중";
    if (!cfg.scanPath.empty())
        std::wcout << L" (경로: " << cfg.scanPath << L")";
    std::wcout << L"...\n";

    auto t_scan_start = std::chrono::steady_clock::now();

    std::vector<FileEntry> files = scanner.scanFiles(
        cfg.scanPath,
        [](DWORD cur, DWORD tot) {
            if (tot > 0 && cur % 10000 == 0) {
                std::wcout << L"\r  조회 중... " << cur << L"/" << tot << L"   ";
                std::wcout.flush();
            }
        }
    );
    std::wcout << L"\r  ✓ 총 " << files.size() << L"개 파일 발견\n\n";

    if (files.empty()) {
        std::wcout << L"스캔할 파일이 없습니다.\n";
        return 0;
    }

    // 파일 유형 통계
    int docCount = 0, imgCount = 0;
    for (const auto& f : files) {
        if (f.isDocument) ++docCount;
        if (f.isImage)    ++imgCount;
    }
    std::wcout << L"  - 문서 파일: " << docCount << L"개\n";
    std::wcout << L"  - 이미지 파일: " << imgCount << L"개\n\n";

    // ---- Step 3: 개인정보 스캔 ----
    int nThreads = (cfg.numThreads > 0) ? cfg.numThreads
        : std::max(1, (int)std::thread::hardware_concurrency());
    std::wcout << L"[3/4] 개인정보 스캔 중 (스레드: " << nThreads << L"개)...\n";

    std::atomic<int> piiFound{0};
    auto t_pii_start = std::chrono::steady_clock::now();

    auto results = scanAll(files, cfg, piiFound);

    auto t_pii_end = std::chrono::steady_clock::now();
    double elapsedSec = std::chrono::duration<double>(
        t_pii_end - t_scan_start).count();

    std::wcout << L"\n  ✓ 스캔 완료\n";
    std::wcout << L"  - 개인정보 탐지: " << piiFound.load() << L"건\n";
    std::wcout << L"  - 소요 시간: "
               << (int)elapsedSec / 60 << L"분 "
               << (int)elapsedSec % 60 << L"초\n\n";

    // ---- Step 4: 리포트 생성 ----
    std::wcout << L"[4/4] 리포트 생성 중...\n";

    // 요약 정보 구성
    ScanSummary summary;
    summary.scanPath          = cfg.scanPath.empty() ? L"전체 드라이브" : cfg.scanPath;
    summary.scanTime          = currentTimeString();
    summary.totalFilesScanned = (int)files.size();
    summary.totalPiiFound     = piiFound.load();
    summary.totalScanSec      = elapsedSec;

    for (const auto& r : results) {
        if (r.totalMatches() > 0) ++summary.filesWithPii;
        for (const auto& m : r.matches)
            summary.piiTypeCounts[m.type]++;
    }

    // 출력 폴더 생성
    CreateDirectoryW(cfg.outputDir.c_str(), nullptr);

    std::wstring baseName = L"pii_report_" + currentTimestamp();
    Reporter reporter;
    if (!reporter.saveAll(results, summary, cfg.outputDir, baseName)) {
        std::wcerr << L"[경고] " << reporter.getLastError() << L"\n";
    }

    std::wstring xlsxPath = cfg.outputDir + L"\\" + baseName + L".xlsx";
    std::wstring htmlPath = cfg.outputDir + L"\\" + baseName + L".html";

    std::wcout << L"  ✓ Excel: " << xlsxPath << L"\n";
    std::wcout << L"  ✓ HTML:  " << htmlPath << L"\n";

    // ---- 최종 요약 출력 ----
    std::wcout << L"\n====================================================\n";
    std::wcout << L"  스캔 결과 요약\n";
    std::wcout << L"====================================================\n";
    std::wcout << L"  총 파일 수:        " << files.size()     << L"\n";
    std::wcout << L"  개인정보 파일 수:  " << summary.filesWithPii << L"\n";
    std::wcout << L"  총 탐지 건수:      " << piiFound.load() << L"\n";

    if (!summary.piiTypeCounts.empty()) {
        std::wcout << L"\n  [유형별 탐지]\n";
        for (const auto& [type, count] : summary.piiTypeCounts) {
            if (count > 0) {
                std::wcout << L"    " << std::setw(14) << std::left
                           << PiiDetector::getTypeName(type)
                           << L": " << count << L"건\n";
            }
        }
    }

    std::wcout << L"\n  소요 시간: "
               << (int)elapsedSec / 60 << L"분 "
               << (int)elapsedSec % 60 << L"초\n";
    std::wcout << L"====================================================\n";

    return 0;
}
