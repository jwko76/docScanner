// reporter.cpp
// Excel + HTML 리포트 생성

#include "reporter.h"
#include <xlsxwriter.h>   // vcpkg: libxlsxwriter
#include <windows.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>

// ============================================================
// 내부 유틸
// ============================================================

std::string Reporter::toUtf8(const std::wstring& ws) {
    if (ws.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string s(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, s.data(), len, nullptr, nullptr);
    return s;
}

std::wstring Reporter::htmlEscape(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size());
    for (wchar_t c : s) {
        switch (c) {
            case L'<':  out += L"&lt;";   break;
            case L'>':  out += L"&gt;";   break;
            case L'&':  out += L"&amp;";  break;
            case L'"':  out += L"&quot;"; break;
            case L'\'': out += L"&#39;";  break;
            default:    out += c;          break;
        }
    }
    return out;
}

std::wstring Reporter::formatFileSize(LONGLONG bytes) {
    if (bytes < 1024) return std::to_wstring(bytes) + L" B";
    if (bytes < 1024*1024) return std::to_wstring(bytes/1024) + L" KB";
    if (bytes < 1024LL*1024*1024) return std::to_wstring(bytes/1024/1024) + L" MB";
    return std::to_wstring(bytes/1024/1024/1024) + L" GB";
}

// ============================================================
// Excel 보고서 (libxlsxwriter)
// ============================================================

bool Reporter::saveExcel(const std::vector<FileScanResult>& results,
                          const ScanSummary& summary,
                          const std::wstring& outputPath) {
    std::string path = toUtf8(outputPath);
    lxw_workbook* wb = workbook_new(path.c_str());
    if (!wb) { m_lastError = L"Excel 파일 생성 실패: " + outputPath; return false; }

    // ---- 서식 정의 ----
    lxw_format* fmtHeader = workbook_add_format(wb);
    format_set_bold(fmtHeader);
    format_set_bg_color(fmtHeader, 0x2F5496);
    format_set_font_color(fmtHeader, LXW_COLOR_WHITE);
    format_set_align(fmtHeader, LXW_ALIGN_CENTER);
    format_set_border(fmtHeader, LXW_BORDER_THIN);

    lxw_format* fmtCell = workbook_add_format(wb);
    format_set_border(fmtCell, LXW_BORDER_THIN);
    format_set_text_wrap(fmtCell);

    lxw_format* fmtCellRed = workbook_add_format(wb);
    format_set_border(fmtCellRed, LXW_BORDER_THIN);
    format_set_font_color(fmtCellRed, 0xC00000);
    format_set_bold(fmtCellRed);

    lxw_format* fmtTitle = workbook_add_format(wb);
    format_set_bold(fmtTitle);
    format_set_font_size(fmtTitle, 14);

    lxw_format* fmtNum = workbook_add_format(wb);
    format_set_border(fmtNum, LXW_BORDER_THIN);
    format_set_align(fmtNum, LXW_ALIGN_RIGHT);

    // ================================================================
    // 시트 1: 요약 (Summary)
    // ================================================================
    lxw_worksheet* wsSummary = workbook_add_worksheet(wb, "요약");
    worksheet_set_column(wsSummary, 0, 0, 25, nullptr);
    worksheet_set_column(wsSummary, 1, 1, 40, nullptr);

    int row = 0;
    worksheet_write_string(wsSummary, row, 0, "개인정보 스캔 결과 요약", fmtTitle);
    row += 2;

    auto writeKV = [&](const char* key, const std::string& val) {
        worksheet_write_string(wsSummary, row, 0, key, fmtHeader);
        worksheet_write_string(wsSummary, row, 1, val.c_str(), fmtCell);
        ++row;
    };

    writeKV("스캔 경로",    toUtf8(summary.scanPath));
    writeKV("스캔 일시",    toUtf8(summary.scanTime));
    writeKV("스캔 파일 수", std::to_string(summary.totalFilesScanned));
    writeKV("개인정보 파일 수", std::to_string(summary.filesWithPii));
    writeKV("개인정보 탐지 건수", std::to_string(summary.totalPiiFound));
    writeKV("소요 시간",
        std::to_string((int)summary.totalScanSec / 60) + "분 " +
        std::to_string((int)summary.totalScanSec % 60) + "초");

    row++;
    worksheet_write_string(wsSummary, row, 0, "유형별 탐지 건수", fmtTitle);
    row++;
    worksheet_write_string(wsSummary, row, 0, "유형", fmtHeader);
    worksheet_write_string(wsSummary, row, 1, "건수", fmtHeader);
    row++;

    for (const auto& [type, count] : summary.piiTypeCounts) {
        if (count == 0) continue;
        std::string typeName = toUtf8(PiiDetector::getTypeName(type));
        worksheet_write_string(wsSummary, row, 0, typeName.c_str(), fmtCell);
        worksheet_write_number(wsSummary, row, 1, count, fmtNum);
        row++;
    }

    // ================================================================
    // 시트 2: 파일 목록 (Files)
    // ================================================================
    lxw_worksheet* wsFiles = workbook_add_worksheet(wb, "파일 목록");
    worksheet_set_column(wsFiles, 0, 0, 60, nullptr);  // 파일 경로
    worksheet_set_column(wsFiles, 1, 1, 8,  nullptr);  // 확장자
    worksheet_set_column(wsFiles, 2, 2, 10, nullptr);  // 탐지 건수
    worksheet_set_column(wsFiles, 3, 3, 12, nullptr);  // 추출 방법
    worksheet_set_column(wsFiles, 4, 4, 12, nullptr);  // 소요 시간
    worksheet_set_column(wsFiles, 5, 5, 30, nullptr);  // 오류 메시지

    const char* fileHeaders[] = {
        "파일 경로", "확장자", "탐지 건수", "추출 방법", "소요(초)", "오류"
    };
    for (int c = 0; c < 6; ++c)
        worksheet_write_string(wsFiles, 0, c, fileHeaders[c], fmtHeader);

    int fr = 1;
    for (const auto& r : results) {
        lxw_format* rowFmt = (r.totalMatches() > 0) ? fmtCellRed : fmtCell;
        worksheet_write_string(wsFiles, fr, 0, toUtf8(r.filePath).c_str(),    fmtCell);
        worksheet_write_string(wsFiles, fr, 1, toUtf8(r.extension).c_str(),   fmtCell);
        worksheet_write_number(wsFiles, fr, 2, r.totalMatches(),               rowFmt);
        worksheet_write_string(wsFiles, fr, 3, toUtf8(r.extractionMethod).c_str(), fmtCell);
        worksheet_write_number(wsFiles, fr, 4, r.scanTimeSec,                  fmtCell);
        worksheet_write_string(wsFiles, fr, 5, toUtf8(r.extractionError).c_str(), fmtCell);
        ++fr;
    }

    // 자동 필터
    worksheet_autofilter(wsFiles, 0, 0, fr, 5);

    // ================================================================
    // 시트 3: 상세 탐지 결과 (Detail)
    // ================================================================
    lxw_worksheet* wsDetail = workbook_add_worksheet(wb, "상세 결과");
    worksheet_set_column(wsDetail, 0, 0, 50, nullptr);  // 파일 경로
    worksheet_set_column(wsDetail, 1, 1, 12, nullptr);  // 탐지 유형
    worksheet_set_column(wsDetail, 2, 2, 20, nullptr);  // 탐지 값
    worksheet_set_column(wsDetail, 3, 3, 20, nullptr);  // 마스킹 값
    worksheet_set_column(wsDetail, 4, 4, 8,  nullptr);  // 줄 번호
    worksheet_set_column(wsDetail, 5, 5, 60, nullptr);  // 맥락
    worksheet_set_column(wsDetail, 6, 6, 8,  nullptr);  // 신뢰도

    const char* detailHeaders[] = {
        "파일 경로", "탐지 유형", "탐지 값", "마스킹 값", "줄 번호", "맥락", "신뢰도"
    };
    for (int c = 0; c < 7; ++c)
        worksheet_write_string(wsDetail, 0, c, detailHeaders[c], fmtHeader);

    int dr = 1;
    for (const auto& r : results) {
        for (const auto& m : r.matches) {
            worksheet_write_string(wsDetail, dr, 0, toUtf8(r.filePath).c_str(),     fmtCell);
            worksheet_write_string(wsDetail, dr, 1, toUtf8(m.typeName).c_str(),     fmtCellRed);
            worksheet_write_string(wsDetail, dr, 2, toUtf8(m.matchedText).c_str(),  fmtCellRed);
            worksheet_write_string(wsDetail, dr, 3, toUtf8(m.maskedText).c_str(),   fmtCell);
            worksheet_write_number(wsDetail, dr, 4, m.lineNumber,                    fmtNum);
            worksheet_write_string(wsDetail, dr, 5, toUtf8(m.contextSnippet).c_str(), fmtCell);
            worksheet_write_number(wsDetail, dr, 6, m.confidence,                    fmtNum);
            ++dr;
        }
    }

    worksheet_autofilter(wsDetail, 0, 0, dr, 6);

    // 저장
    lxw_error err = workbook_close(wb);
    if (err) {
        m_lastError = L"Excel 저장 실패 (오류코드=" +
                       std::to_wstring(err) + L")";
        return false;
    }
    return true;
}

// ============================================================
// HTML 보고서
// ============================================================

std::wstring Reporter::buildHtmlPage(const std::vector<FileScanResult>& results,
                                      const ScanSummary& summary) {
    std::wostringstream html;

    html << LR"(<!DOCTYPE html>
<html lang="ko">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>개인정보 스캔 결과</title>
<style>
  body{font-family:'맑은 고딕',sans-serif;margin:0;background:#f4f6f9;color:#333}
  .header{background:linear-gradient(135deg,#1e3a5f,#2f5496);color:#fff;padding:24px 32px}
  .header h1{margin:0;font-size:24px}
  .header p{margin:4px 0 0;opacity:.8;font-size:13px}
  .container{max-width:1400px;margin:24px auto;padding:0 16px}
  .summary-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:16px;margin-bottom:24px}
  .card{background:#fff;border-radius:8px;padding:16px 20px;box-shadow:0 2px 8px rgba(0,0,0,.08)}
  .card .val{font-size:28px;font-weight:700;color:#2f5496}
  .card .lbl{font-size:12px;color:#888;margin-top:4px}
  .card.danger .val{color:#c00000}
  .section{background:#fff;border-radius:8px;box-shadow:0 2px 8px rgba(0,0,0,.08);margin-bottom:24px;overflow:hidden}
  .section-header{background:#2f5496;color:#fff;padding:12px 20px;font-weight:600}
  table{width:100%;border-collapse:collapse;font-size:13px}
  th{background:#dce6f1;padding:8px 12px;text-align:left;font-weight:600;position:sticky;top:0}
  td{padding:7px 12px;border-bottom:1px solid #eee;vertical-align:top;word-break:break-all}
  tr:hover td{background:#f0f4fa}
  .badge{display:inline-block;padding:2px 8px;border-radius:10px;font-size:11px;font-weight:600;white-space:nowrap}
  .badge-ssn{background:#fde9e9;color:#c00000}
  .badge-phone{background:#e9f0fd;color:#1a5276}
  .badge-email{background:#e9fde9;color:#196f3d}
  .badge-ip{background:#fdf5e9;color:#784212}
  .badge-mac{background:#f5e9fd;color:#512e5f}
  .badge-card{background:#fef9e7;color:#7d6608}
  .badge-addr{background:#e9fdfd;color:#0e6655}
  .badge-bank{background:#fdecea;color:#922b21}
  .badge-other{background:#eee;color:#555}
  .masked{color:#888;font-style:italic}
  .context{font-size:11px;color:#666;max-width:400px;white-space:pre-wrap}
  .file-ok{color:#888}
  .file-hit{color:#c00000;font-weight:600}
  .count-0{color:#bbb}
  .count-pos{color:#c00000;font-weight:700}
  .tab-btn{padding:8px 20px;cursor:pointer;background:#dce6f1;border:none;font-size:13px;border-radius:4px 4px 0 0;margin-right:4px}
  .tab-btn.active{background:#2f5496;color:#fff}
  .tab-panel{display:none}.tab-panel.active{display:block}
  .chart-bar{display:flex;align-items:center;gap:8px;margin:6px 0}
  .chart-bar .bar{height:20px;background:#2f5496;border-radius:3px;min-width:4px}
  .chart-bar .txt{font-size:12px;color:#333}
  @media print{.header{-webkit-print-color-adjust:exact}}
</style>
</head>
<body>
<div class="header">
  <h1>&#128274; 개인정보 스캔 결과</h1>
  <p>스캔 경로: )";
    html << htmlEscape(summary.scanPath);
    html << L" &nbsp;|&nbsp; 실행 시각: " << htmlEscape(summary.scanTime);
    html << LR"(</p>
</div>
<div class="container">
)";

    // ---- 요약 카드 ----
    html << L"<div class=\"summary-grid\">\n";
    auto card = [&](const std::wstring& val, const std::wstring& lbl,
                    bool danger = false) {
        html << L"  <div class=\"card" << (danger ? L" danger" : L"") << L"\">"
             << L"<div class=\"val\">" << val << L"</div>"
             << L"<div class=\"lbl\">" << lbl << L"</div></div>\n";
    };
    card(std::to_wstring(summary.totalFilesScanned), L"스캔 파일");
    card(std::to_wstring(summary.filesWithPii),      L"개인정보 파일", true);
    card(std::to_wstring(summary.totalPiiFound),     L"탐지 건수",    true);
    // 소요시간
    std::wstring elapsed =
        std::to_wstring((int)summary.totalScanSec / 60) + L"분 " +
        std::to_wstring((int)summary.totalScanSec % 60) + L"초";
    card(elapsed, L"소요 시간");
    html << L"</div>\n";

    // ---- 탭 ----
    html << LR"(<div style="margin-bottom:0">
  <button class="tab-btn active" onclick="switchTab(0)">유형별 통계</button>
  <button class="tab-btn" onclick="switchTab(1)">파일 목록</button>
  <button class="tab-btn" onclick="switchTab(2)">상세 결과</button>
</div>
)";

    // ---- 탭1: 유형별 통계 ----
    html << L"<div class=\"tab-panel active\" id=\"tab0\">\n";
    html << L"<div class=\"section\"><div class=\"section-header\">유형별 탐지 건수</div>\n";
    html << L"<div style=\"padding:16px\">\n";

    int maxCount = 1;
    for (const auto& [t, c] : summary.piiTypeCounts) maxCount = std::max(maxCount, c);

    for (const auto& [type, count] : summary.piiTypeCounts) {
        if (count == 0) continue;
        int barW = count * 400 / maxCount;
        html << L"<div class=\"chart-bar\">"
             << L"<div style=\"width:120px;text-align:right;font-size:13px\">"
             << htmlEscape(PiiDetector::getTypeName(type)) << L"</div>"
             << L"<div class=\"bar\" style=\"width:" << barW << L"px\"></div>"
             << L"<div class=\"txt\">" << count << L"건</div></div>\n";
    }
    html << L"</div></div></div>\n";

    // ---- 탭2: 파일 목록 ----
    html << L"<div class=\"tab-panel\" id=\"tab1\">\n";
    html << L"<div class=\"section\"><div class=\"section-header\">파일 목록</div>\n";
    html << L"<table><tr><th>파일 경로</th><th>확장자</th><th>탐지</th>"
            L"<th>추출 방법</th><th>소요(초)</th><th>오류</th></tr>\n";
    for (const auto& r : results) {
        bool hit = r.totalMatches() > 0;
        html << L"<tr><td class=\"" << (hit ? L"file-hit" : L"file-ok") << L"\">"
             << htmlEscape(r.filePath) << L"</td>"
             << L"<td>" << htmlEscape(r.extension) << L"</td>"
             << L"<td class=\"" << (hit ? L"count-pos" : L"count-0") << L"\">"
             << r.totalMatches() << L"</td>"
             << L"<td>" << htmlEscape(r.extractionMethod) << L"</td>"
             << L"<td>" << std::fixed;

        std::wostringstream tmp;
        tmp << std::fixed << std::setprecision(2) << r.scanTimeSec;
        html << tmp.str();

        html << L"</td>"
             << L"<td style=\"color:#c00\">" << htmlEscape(r.extractionError) << L"</td></tr>\n";
    }
    html << L"</table></div></div>\n";

    // ---- 탭3: 상세 결과 ----
    html << L"<div class=\"tab-panel\" id=\"tab2\">\n";
    html << L"<div class=\"section\"><div class=\"section-header\">상세 탐지 결과</div>\n";
    html << L"<table><tr><th>파일</th><th>유형</th><th>탐지 값</th>"
            L"<th>마스킹</th><th>줄</th><th>맥락</th></tr>\n";

    auto typeBadge = [](PiiType t) -> std::wstring {
        switch (t) {
            case PiiType::RESIDENT_NUMBER: return L"badge-ssn";
            case PiiType::PHONE_NUMBER:    return L"badge-phone";
            case PiiType::EMAIL:           return L"badge-email";
            case PiiType::IP_ADDRESS:      return L"badge-ip";
            case PiiType::MAC_ADDRESS:     return L"badge-mac";
            case PiiType::CREDIT_CARD:     return L"badge-card";
            case PiiType::ADDRESS:         return L"badge-addr";
            case PiiType::BANK_ACCOUNT:    return L"badge-bank";
            default:                       return L"badge-other";
        }
    };

    for (const auto& r : results) {
        for (const auto& m : r.matches) {
            html << L"<tr>"
                 << L"<td>" << htmlEscape(r.filePath) << L"</td>"
                 << L"<td><span class=\"badge " << typeBadge(m.type) << L"\">"
                 << htmlEscape(m.typeName) << L"</span></td>"
                 << L"<td style=\"color:#c00;font-weight:600\">"
                 << htmlEscape(m.matchedText) << L"</td>"
                 << L"<td class=\"masked\">" << htmlEscape(m.maskedText) << L"</td>"
                 << L"<td style=\"text-align:center\">" << m.lineNumber << L"</td>"
                 << L"<td class=\"context\">" << htmlEscape(m.contextSnippet) << L"</td>"
                 << L"</tr>\n";
        }
    }
    html << L"</table></div></div>\n";

    // ---- 스크립트 ----
    html << LR"(</div>
<script>
function switchTab(idx){
  document.querySelectorAll('.tab-panel').forEach((p,i)=>{
    p.classList.toggle('active', i===idx);
  });
  document.querySelectorAll('.tab-btn').forEach((b,i)=>{
    b.classList.toggle('active', i===idx);
  });
}
</script>
</body></html>)";

    return html.str();
}

bool Reporter::saveHtml(const std::vector<FileScanResult>& results,
                         const ScanSummary& summary,
                         const std::wstring& outputPath) {
    std::wstring html = buildHtmlPage(results, summary);

    // UTF-8 BOM으로 저장
    std::ofstream ofs(outputPath, std::ios::binary);
    if (!ofs) { m_lastError = L"HTML 파일 쓰기 실패: " + outputPath; return false; }
    // BOM
    uint8_t bom[] = { 0xEF, 0xBB, 0xBF };
    ofs.write(reinterpret_cast<const char*>(bom), 3);
    // 내용
    std::string utf8 = toUtf8(html);
    ofs.write(utf8.c_str(), utf8.size());
    return true;
}

// ============================================================
// 두 리포트 동시 저장
// ============================================================

bool Reporter::saveAll(const std::vector<FileScanResult>& results,
                        const ScanSummary& summary,
                        const std::wstring& outputDir,
                        const std::wstring& baseName) {
    bool ok = true;
    ok &= saveExcel(results, summary, outputDir + L"\\" + baseName + L".xlsx");
    ok &= saveHtml(results,  summary, outputDir + L"\\" + baseName + L".html");
    return ok;
}
