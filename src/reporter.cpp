// reporter.cpp
// Excel + HTML 리포트 생성

#include "reporter.h"
#include "xlsx_writer.h"  // zero-dependency xlsx writer (no libxlsxwriter)
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

// Windows 경로 → file:/// URL 변환 (역슬래시 → 슬래시)
static std::wstring pathToFileUrl(const std::wstring& path) {
    std::wstring url = L"file:///";
    for (wchar_t c : path) {
        url += (c == L'\\') ? L'/' : c;
    }
    return url;
}

// 맥락 문자열 정리: 출력 불가능한 제어 문자 제거 (한글 안전)
static std::wstring sanitizeContext(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size());
    for (wchar_t c : s) {
        if (c == L'\0') continue;                 // 널 문자 제거
        if (c < 0x20 && c != L' ') {              // 제어 문자 → 공백
            out += L' ';
        } else {
            out += c;
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
// Excel 보고서 (xlsx_writer.h – zero dependency)
// ============================================================

bool Reporter::saveExcel(const std::vector<FileScanResult>& results,
                          const ScanSummary& summary,
                          const std::wstring& outputPath) {
    std::string path = toUtf8(outputPath);

    XlsxWriter wb;

    // ================================================================
    // 시트 1: 요약 (Summary)
    // ================================================================
    int shSum = wb.addWorksheet(toUtf8(L"요약"));
    wb.setColumnWidth(shSum, 0, 25.0);
    wb.setColumnWidth(shSum, 1, 40.0);

    int row = 0;
    wb.writeString(shSum, row, 0, toUtf8(L"개인정보 스캔 결과 요약"), XLFMT_TITLE);
    row += 2;

    auto writeKV = [&](const std::wstring& key, const std::string& val) {
        wb.writeString(shSum, row, 0, toUtf8(key), XLFMT_HEADER);
        wb.writeString(shSum, row, 1, val,          XLFMT_CELL);
        ++row;
    };

    writeKV(L"스캔 경로",       toUtf8(summary.scanPath));
    writeKV(L"스캔 일시",       toUtf8(summary.scanTime));
    writeKV(L"스캔 파일 수",    std::to_string(summary.totalFilesScanned));
    writeKV(L"개인정보 파일 수",std::to_string(summary.filesWithPii));
    writeKV(L"개인정보 탐지 건수", std::to_string(summary.totalPiiFound));
    writeKV(L"소요 시간",
        std::to_string((int)summary.totalScanSec / 60) + toUtf8(L"분 ") +
        std::to_string((int)summary.totalScanSec % 60) + toUtf8(L"초"));

    row++;
    wb.writeString(shSum, row, 0, toUtf8(L"유형별 탐지 건수"), XLFMT_TITLE);
    row++;
    wb.writeString(shSum, row, 0, toUtf8(L"유형"), XLFMT_HEADER);
    wb.writeString(shSum, row, 1, toUtf8(L"건수"), XLFMT_HEADER);
    row++;

    for (const auto& tc : summary.piiTypeCounts) {
        if (tc.second == 0) continue;
        wb.writeString(shSum, row, 0, toUtf8(PiiDetector::getTypeName(tc.first)), XLFMT_CELL);
        wb.writeNumber(shSum, row, 1, tc.second, XLFMT_NUM);
        row++;
    }

    // ================================================================
    // 시트 2: 파일 목록 (Files)
    // ================================================================
    int shFiles = wb.addWorksheet(toUtf8(L"파일 목록"));
    wb.setColumnWidth(shFiles, 0, 60.0);
    wb.setColumnWidth(shFiles, 1,  8.0);
    wb.setColumnWidth(shFiles, 2, 10.0);
    wb.setColumnWidth(shFiles, 3, 35.0);
    wb.setColumnWidth(shFiles, 4, 12.0);
    wb.setColumnWidth(shFiles, 5, 12.0);
    wb.setColumnWidth(shFiles, 6, 30.0);

    wb.writeString(shFiles, 0, 0, toUtf8(L"파일 경로"),  XLFMT_HEADER);
    wb.writeString(shFiles, 0, 1, toUtf8(L"확장자"),    XLFMT_HEADER);
    wb.writeString(shFiles, 0, 2, toUtf8(L"탐지 건수"), XLFMT_HEADER);
    wb.writeString(shFiles, 0, 3, toUtf8(L"탐지 유형"), XLFMT_HEADER);
    wb.writeString(shFiles, 0, 4, toUtf8(L"추출 방법"), XLFMT_HEADER);
    wb.writeString(shFiles, 0, 5, toUtf8(L"소요(초)"),  XLFMT_HEADER);
    wb.writeString(shFiles, 0, 6, toUtf8(L"오류"),      XLFMT_HEADER);

    int fr = 1;
    for (const auto& r : results) {
        int cntFmt = (r.totalMatches() > 0) ? XLFMT_CELL_RED : XLFMT_NUM;
        // 파일 경로: 클릭 시 열리는 HYPERLINK 수식
        std::string url  = toUtf8(pathToFileUrl(r.filePath));
        std::string disp = toUtf8(r.filePath);
        wb.writeHyperlink(shFiles, fr, 0, url, disp);
        wb.writeString(shFiles, fr, 1, toUtf8(r.extension),       XLFMT_CELL);
        wb.writeNumber(shFiles, fr, 2, r.totalMatches(),           cntFmt);

        // 탐지 유형: 고유 유형 쉼표 구분 문자열
        std::wstring typeList;
        std::vector<PiiType> seenTypes;
        for (const auto& m : r.matches) {
            bool already = false;
            for (auto t : seenTypes) if (t == m.type) { already = true; break; }
            if (!already) {
                seenTypes.push_back(m.type);
                if (!typeList.empty()) typeList += L", ";
                typeList += PiiDetector::getTypeName(m.type);
            }
        }
        wb.writeString(shFiles, fr, 3, toUtf8(typeList),          cntFmt);
        wb.writeString(shFiles, fr, 4, toUtf8(r.extractionMethod),XLFMT_CELL);
        wb.writeNumber(shFiles, fr, 5, r.scanTimeSec,              XLFMT_CELL);
        wb.writeString(shFiles, fr, 6, toUtf8(r.extractionError), XLFMT_CELL);
        ++fr;
    }
    wb.setAutoFilter(shFiles, 0, 0, fr, 6);

    // ================================================================
    // 시트 3: 상세 탐지 결과 (Detail)
    // ================================================================
    int shDetail = wb.addWorksheet(toUtf8(L"상세 결과"));
    wb.setColumnWidth(shDetail, 0, 50.0);
    wb.setColumnWidth(shDetail, 1, 12.0);
    wb.setColumnWidth(shDetail, 2, 20.0);
    wb.setColumnWidth(shDetail, 3, 20.0);
    wb.setColumnWidth(shDetail, 4,  8.0);
    wb.setColumnWidth(shDetail, 5, 60.0);
    wb.setColumnWidth(shDetail, 6,  8.0);

    wb.writeString(shDetail, 0, 0, toUtf8(L"파일 경로"),  XLFMT_HEADER);
    wb.writeString(shDetail, 0, 1, toUtf8(L"탐지 유형"),  XLFMT_HEADER);
    wb.writeString(shDetail, 0, 2, toUtf8(L"탐지 값"),    XLFMT_HEADER);
    wb.writeString(shDetail, 0, 3, toUtf8(L"마스킹 값"),  XLFMT_HEADER);
    wb.writeString(shDetail, 0, 4, toUtf8(L"줄 번호"),    XLFMT_HEADER);
    wb.writeString(shDetail, 0, 5, toUtf8(L"맥락"),       XLFMT_HEADER);
    wb.writeString(shDetail, 0, 6, toUtf8(L"신뢰도"),     XLFMT_HEADER);

    int dr = 1;
    for (const auto& r : results) {
        for (const auto& m : r.matches) {
            wb.writeHyperlink(shDetail, dr, 0,
                toUtf8(pathToFileUrl(r.filePath)), toUtf8(r.filePath));
            wb.writeString(shDetail, dr, 1, toUtf8(m.typeName),       XLFMT_CELL_RED);
            wb.writeString(shDetail, dr, 2, toUtf8(m.matchedText),    XLFMT_CELL_RED);
            wb.writeString(shDetail, dr, 3, toUtf8(m.maskedText),     XLFMT_CELL);
            wb.writeNumber(shDetail, dr, 4, m.lineNumber,              XLFMT_NUM);
            wb.writeString(shDetail, dr, 5, toUtf8(m.contextSnippet), XLFMT_CELL);
            wb.writeNumber(shDetail, dr, 6, m.confidence,              XLFMT_NUM);
            ++dr;
        }
    }
    wb.setAutoFilter(shDetail, 0, 0, dr, 6);

    // 저장
    if (!wb.save(path)) {
        m_lastError = L"Excel 저장 실패: " + outputPath;
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
    // NOTE: Use named raw-string delimiter "TAB" because the HTML content
    // contains )" (from onclick="switchTab(N)") which would prematurely close R"(...)".
    html << LR"TAB(<div style="margin-bottom:0">
  <button class="tab-btn active" onclick="switchTab(0)">유형별 통계</button>
  <button class="tab-btn" onclick="switchTab(1)">파일 목록</button>
  <button class="tab-btn" onclick="switchTab(2)">상세 결과</button>
</div>
)TAB";

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

    // 유형 → CSS 배지 클래스 매핑 (파일 목록 + 상세 결과 공통 사용)
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

    // ---- 탭2: 파일 목록 ----
    html << L"<div class=\"tab-panel\" id=\"tab1\">\n";
    html << L"<div class=\"section\"><div class=\"section-header\">파일 목록</div>\n";
    html << L"<table><tr><th>파일 경로</th><th>확장자</th><th>탐지</th>"
            L"<th>탐지 유형</th><th>추출 방법</th><th>소요(초)</th><th>오류</th></tr>\n";
    for (const auto& r : results) {
        bool hit = r.totalMatches() > 0;

        // 해당 파일에서 검출된 고유 PII 유형 수집 (순서 유지)
        std::vector<PiiType> seenTypes;
        for (const auto& m : r.matches) {
            bool already = false;
            for (auto t : seenTypes) if (t == m.type) { already = true; break; }
            if (!already) seenTypes.push_back(m.type);
        }

        html << L"<tr><td class=\"" << (hit ? L"file-hit" : L"file-ok") << L"\">"
             << L"<a href=\"" << pathToFileUrl(r.filePath)
             << L"\" title=\"" << htmlEscape(r.filePath) << L"\" style=\"color:inherit;text-decoration:underline dotted\">"
             << htmlEscape(r.filePath) << L"</a></td>"
             << L"<td>" << htmlEscape(r.extension) << L"</td>"
             << L"<td class=\"" << (hit ? L"count-pos" : L"count-0") << L"\">"
             << r.totalMatches() << L"</td>"
             << L"<td>";
        for (PiiType t : seenTypes) {
            html << L"<span class=\"badge " << typeBadge(t) << L"\">"
                 << htmlEscape(PiiDetector::getTypeName(t)) << L"</span> ";
        }
        html << L"</td>"
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

    for (const auto& r : results) {
        for (const auto& m : r.matches) {
            // 파일명만 추출 (경로 전체는 title에)
            std::wstring fileName = r.filePath;
            auto slash = fileName.find_last_of(L"\\/");
            if (slash != std::wstring::npos) fileName = fileName.substr(slash + 1);

            html << L"<tr>"
                 << L"<td><a href=\"" << pathToFileUrl(r.filePath)
                 << L"\" title=\"" << htmlEscape(r.filePath)
                 << L"\" style=\"color:#c00000;font-weight:600;text-decoration:underline dotted\">"
                 << htmlEscape(fileName) << L"</a></td>"
                 << L"<td><span class=\"badge " << typeBadge(m.type) << L"\">"
                 << htmlEscape(m.typeName) << L"</span></td>"
                 << L"<td style=\"color:#c00;font-weight:600\">"
                 << htmlEscape(m.matchedText) << L"</td>"
                 << L"<td class=\"masked\">" << htmlEscape(m.maskedText) << L"</td>"
                 << L"<td style=\"text-align:center\">" << m.lineNumber << L"</td>"
                 << L"<td class=\"context\">" << htmlEscape(sanitizeContext(m.contextSnippet)) << L"</td>"
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
