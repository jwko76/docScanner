#pragma once
// reporter.h
// 스캔 결과를 Excel(.xlsx) + HTML 리포트로 출력

#include "pii_detector.h"
#include <string>
#include <vector>
#include <chrono>

struct ScanSummary {
    std::wstring  scanPath;
    std::wstring  scanTime;           // 실행 시각 (문자열)
    int           totalFilesScanned;
    int           filesWithPii;
    int           totalPiiFound;
    double        totalScanSec;
    std::map<PiiType, int> piiTypeCounts;
};

class Reporter {
public:
    Reporter() = default;

    // Excel 보고서 저장
    // outputPath: 예) L"C:\\Reports\\pii_report.xlsx"
    bool saveExcel(const std::vector<FileScanResult>& results,
                   const ScanSummary& summary,
                   const std::wstring& outputPath);

    // HTML 보고서 저장
    bool saveHtml(const std::vector<FileScanResult>& results,
                  const ScanSummary& summary,
                  const std::wstring& outputPath);

    // 두 리포트 동시 저장
    bool saveAll(const std::vector<FileScanResult>& results,
                 const ScanSummary& summary,
                 const std::wstring& outputDir,
                 const std::wstring& baseName = L"pii_report");

    std::wstring getLastError() const { return m_lastError; }

private:
    std::wstring m_lastError;

    // HTML 헬퍼
    std::wstring buildHtmlPage(const std::vector<FileScanResult>& results,
                                const ScanSummary& summary);
    std::wstring htmlEscape(const std::wstring& s);
    std::wstring formatFileSize(LONGLONG bytes);

    // Excel 헬퍼 (libxlsxwriter 이용)
    // wstring → char* 변환 (UTF-8)
    std::string toUtf8(const std::wstring& ws);
};
