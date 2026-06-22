# PiiScanner — 구현 문서 (개발자 가이드)

> 대상 독자: 코드를 유지보수·확장하는 개발자  
> 빌드 환경: MSVC 19.51+, Windows SDK 10.0.26100+, C++20

---

## 1. 아키텍처 개요

```
CLI (main.cpp)
    │
    ├── EverythingScanner          파일 목록 수집 (IPC)
    │       └── Everything64.dll  voidtools Everything SDK
    │
    ├── TextExtractor              텍스트 추출 (파일별)
    │       ├── PlainText          직접 파일 읽기 + 인코딩 감지
    │       ├── IFilter (COM)      Office/PDF/HWP → Windows 필터
    │       ├── OOXML Direct       docx/xlsx/pptx ZIP+XML fallback
    │       └── WinOCR (WinRT)    이미지 OCR (Windows.Media.Ocr)
    │
    ├── PiiDetector                PII 패턴 탐지
    │       ├── wregex 패턴        유형별 정규식 매칭
    │       └── 체크섬/알고리즘    주민등록번호·신용카드·IP 검증
    │
    └── Reporter                   결과 저장
            ├── xlsx_writer.h      Excel 3시트 (의존성 없음)
            └── HTML               인라인 CSS+JS 대화형 리포트
```

**스레드 모델**: 파일 목록은 싱글 스레드로 수집, 텍스트 추출·PII 탐지는  
`std::thread` 풀로 병렬 처리. 각 스레드는 독립된 `TextExtractor`/`PiiDetector`  
인스턴스를 소유 (instance-level thread safety). 결과 수집은 `std::mutex`로 보호.

**부하 수준 (GUI)**: `ScanConfig.loadLevel` (0=낮음/1=중간/2=높음) 에 따라  
스레드 수(코어의 25%/50%/100%)와 `SetThreadPriority()`를 동시에 조정.  
기본값은 중간(~50%)으로 백그라운드 작업에 영향을 최소화.

---

## 2. 모듈 API 레퍼런스

### 2.1 `EverythingScanner`

```cpp
#include "everything_scanner.h"

EverythingScanner scanner;

// DLL 로드 및 Everything 연결 확인
bool ok = scanner.initialize(L"");           // 빈 문자열 = exe 폴더 자동 탐색
bool ok = scanner.initialize(L"C:\\Tools\\Everything64.dll"); // 경로 직접 지정

// 파일 목록 수집
std::vector<FileEntry> files = scanner.scanFiles(
    L"C:\\Users\\jwko",                       // 루트 경로 (빈 문자열 = 전체 드라이브)
    [](int done, int total) { /* 진행률 */ }  // 선택적 콜백
);

// 오류 메시지 확인
std::wstring err = scanner.getLastError();
```

**`FileEntry` 구조체**

```cpp
struct FileEntry {
    std::wstring fullPath;      // 전체 경로 (예: L"C:\Users\...\file.docx")
    std::wstring extension;     // 소문자 확장자 (예: L".docx")
    LONGLONG     fileSize;      // 바이트 단위
    FILETIME     dateModified;  // 마지막 수정 시각
    bool         isDocument;    // documentExtensions() 포함 여부
    bool         isImage;       // imageExtensions() 포함 여부
};
```

---

### 2.2 `TextExtractor`

```cpp
#include "text_extractor.h"

TextExtractor extractor;   // 스레드당 1개 인스턴스

ExtractionResult result = extractor.extract(
    L"C:\\Users\\..\\report.xlsx",   // 파일 전체 경로
    L".xlsx"                          // 소문자 확장자
);

if (result.success) {
    std::wstring& text = result.text;     // 추출된 텍스트 (wstring)
    std::wstring& method = result.method; // "PlainText" | "IFilter" | "OOXML-Direct" | "WinOCR"
} else {
    std::wstring& err = result.errorMessage;
}
```

**추출 우선순위 매핑**

| 확장자 | 1차 방법 | 2차 (fallback) |
|--------|---------|--------------|
| `.txt` `.log` `.csv` `.xml` `.json` `.html` `.ini` `.cfg` | PlainText | — |
| `.docx` `.xlsx` `.pptx` | IFilter | OOXML-Direct (ZIP+XML 파싱) |
| `.doc` `.xls` `.pdf` `.hwp` `.hwpx` `.ppt` `.rtf` | IFilter | — |
| `.jpg` `.png` `.tif` `.bmp` `.gif` `.webp` | WinOCR | — |

**설정 변경**

```cpp
extractor.setMaxTextLength(2 * 1024 * 1024);  // 기본 2MB
```

---

### 2.3 `PiiDetector`

```cpp
#include "pii_detector.h"

PiiDetector detector;   // 스레드당 1개 인스턴스

std::vector<PiiMatch> matches = detector.detect(text);

for (const PiiMatch& m : matches) {
    m.type;           // PiiType 열거형
    m.typeName;       // L"주민등록번호", L"신용카드", ...
    m.matchedText;    // 원문 매칭 값
    m.maskedText;     // 마스킹 처리된 값
    m.position;       // 텍스트 내 바이트 위치
    m.lineNumber;     // 1-based 줄 번호
    m.contextSnippet; // 앞뒤 문맥 (기본 80자)
    m.confidence;     // 0.0f ~ 1.0f
}
```

**`PiiType` 열거형**

```cpp
enum class PiiType {
    RESIDENT_NUMBER,  // 주민등록번호
    PHONE_NUMBER,     // 전화번호
    EMAIL,            // 이메일
    IP_ADDRESS,       // IP 주소
    MAC_ADDRESS,      // MAC 주소
    CREDIT_CARD,      // 신용카드
    ADDRESS,          // 한국 주소
    BANK_ACCOUNT,     // 계좌번호
    PASSPORT,         // 여권번호
    DRIVER_LICENSE,   // 운전면허번호
};
```

**유형별 활성화/비활성화**

```cpp
detector.setEnabled(PiiType::ADDRESS, false);  // 주소 탐지 비활성화
detector.setMaxMatches(5000);                   // 기본 10000
```

**정적 유틸**

```cpp
std::wstring name = PiiDetector::getTypeName(PiiType::RESIDENT_NUMBER);
// → L"주민등록번호"
```

---

### 2.4 `Reporter`

```cpp
#include "reporter.h"

Reporter reporter;

// Excel + HTML 동시 저장
bool ok = reporter.saveAll(results, summary, L"C:\\Reports", L"pii_report_20260620");
// → pii_report_20260620.xlsx
// → pii_report_20260620.html

// 개별 저장
reporter.saveExcel(results, summary, L"C:\\Reports\\out.xlsx");
reporter.saveHtml(results,  summary, L"C:\\Reports\\out.html");

std::wstring err = reporter.getLastError();
```

**`ScanSummary` 구조체**

```cpp
struct ScanSummary {
    std::wstring scanPath;
    std::wstring scanTime;
    int          totalFilesScanned;
    int          filesWithPii;
    int          totalPiiFound;
    double       totalScanSec;
    std::map<PiiType, int> piiTypeCounts;
};
```

---

### 2.5 `XlsxWriter` (`xlsx_writer.h`)

헤더 전용(header-only), 외부 의존성 없음.

```cpp
#include "xlsx_writer.h"

XlsxWriter wb;
int sheet = wb.addWorksheet("시트명");        // UTF-8 시트명
wb.setColumnWidth(sheet, 0, 30.0);            // col=0, 너비=30
wb.writeString(sheet, 0, 0, "헤더", XLFMT_HEADER);
wb.writeString(sheet, 1, 0, "내용", XLFMT_CELL);
wb.writeNumber(sheet, 1, 1, 42.0,   XLFMT_NUM);
wb.setAutoFilter(sheet, 0, 0, lastRow, lastCol);
wb.save("output.xlsx");                        // UTF-8 경로
```

**셀 포맷 상수**

| 상수 | 외관 |
|------|------|
| `XLFMT_DEFAULT` | 스타일 없음 |
| `XLFMT_HEADER` | 굵은 흰 글자, 파란 배경, 테두리 |
| `XLFMT_CELL` | 테두리, 줄바꿈 |
| `XLFMT_CELL_RED` | 굵은 빨간 글자, 테두리 |
| `XLFMT_NUM` | 우정렬 숫자, 테두리 |
| `XLFMT_TITLE` | 굵은 14pt |


---

## 3. 빌드 시스템

### 3.1 `build.bat` 방식 (권장)

```bat
build.bat
```

내부적으로 `vswhere.exe`로 MSVC를 자동 탐색하고 직접 `cl.exe`를 호출합니다.

**주요 컴파일 플래그**

| 플래그 | 목적 |
|--------|------|
| `/std:c++20` | C++20 표준 (WinRT 코루틴, `std::atomic` 개선) |
| `/MT` | 정적 CRT 링크 — MSVCRT*.dll 불필요 |
| `/utf-8` | 소스/실행 문자셋 UTF-8 |
| `/EHsc` | C++ 예외 처리 활성화 |
| `/O2` | 속도 최적화 |
| `/DUNICODE /D_UNICODE` | Windows Unicode API (`W` 접미사) |
| `/DNOMINMAX` | Windows `min/max` 매크로 비활성화 |
| `/DWIN32_LEAN_AND_MEAN` | 불필요한 Windows 헤더 제외 |

**링크 라이브러리**

| 라이브러리 | 용도 |
|-----------|------|
| `shlwapi.lib` | `PathRemoveFileSpecW` 등 |
| `pathcch.lib` | 현대적 경로 처리 |
| `ole32.lib` | COM 초기화 |
| `oleaut32.lib` | OLE Automation |
| `query.lib` | `LoadIFilter` (Windows IFilter) |
| `windowsapp.lib` | WinRT (`Windows.Media.Ocr` 등) |
| `shell32.lib` | Shell 함수 |

### 3.2 컴파일 시간

`text_extractor.cpp`는 WinRT 헤더(`winrt/base.h`) 포함으로 컴파일에 **2~5분** 소요됩니다.  
CMake를 사용하면 증분 빌드(`cmake --build . --config Release`)로 이 시간을 절약할 수 있습니다.

---

## 4. 의존성 없는 xlsx 구현 상세 (`xlsx_writer.h`)

### ZIP STORED 포맷

Excel(.xlsx) 파일은 ZIP 아카이브입니다.  
`xlsx_writer.h`는 압축 없는 **ZIP STORED** 방식으로 구현하여 zlib 의존성을 제거했습니다.

```
[Local File Header]  signature(4) + version(2) + flags(2) + compression(2=STORED)
                     + mod_time(2) + mod_date(2) + crc32(4)
                     + compressed_size(4) + uncompressed_size(4)
                     + filename_len(2) + extra_len(2)
[File Name]
[File Data]          (비압축 원문)

...반복...

[Central Directory]  각 파일의 중앙 디렉터리 레코드
[End of Central Directory]
```

### CRC32 계산

```cpp
static std::array<uint32_t, 256> makeCrcTable() {
    // 다항식 0xEDB88320 (reversed IEEE 802.3)
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int j = 0; j < 8; ++j)
            c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
        table[i] = c;
    }
}
uint32_t crc = 0xFFFFFFFF;
for (uint8_t b : data) crc = table[(crc ^ b) & 0xFF] ^ (crc >> 8);
crc ^= 0xFFFFFFFF;  // finalize
```

### XML 이스케이프

공유 문자열(sharedStrings.xml)에 삽입되는 셀 내용은 `xe()` 함수로 이스케이프됩니다:
- `&` → `&amp;`
- `<` → `&lt;`
- `>` → `&gt;`
- `"` → `&quot;`
- `0x00`~`0x1F` (탭·LF·CR 제외) → 제거 (OpenXML 규격상 허용 불가)

---

## 5. PII 탐지 알고리즘 상세

### 5.1 주민등록번호 체크섬

```
가중치: 2 3 4 5 6 7 8 9 2 3 4 5   (12자리)
sum = Σ(digit[i] × weight[i])
check_digit = (11 - (sum % 11)) % 10
→ check_digit == digit[12]
```

날짜 유효성: month ∈ [1,12], day ∈ [1,31]  
(윤년·월별 일수는 검증하지 않음 — 오탐 최소화 우선)

### 5.2 신용카드 Luhn 알고리즘

```
오른쪽부터 홀수 위치: 그대로 합산
오른쪽부터 짝수 위치: ×2 후 9 초과 시 -9
총합 % 10 == 0 → 유효
```

### 5.3 IP 주소 필터링

다음 주소는 개인정보가 아니므로 제외:
- `0.0.0.0`
- `255.255.x.x` (브로드캐스트)

※ 사설 주소(192.168.x.x, 10.x.x.x)는 내부망 IP로 개인정보가 될 수 있어 포함.

### 5.4 한국 주소 키워드 매칭

광역시/도 키워드(서울, 부산, 경기 등)가 선행할 때만 트리거하여 오탐을 줄입니다.  
매칭 후 8자 미만인 경우 제외 (너무 짧은 키워드 단독 매칭 방지).

---

## 6. 패턴 추가 방법

`pii_detector.cpp`의 `initPatterns()` 함수에 `PatternDef`를 추가합니다:

```cpp
// 새 유형을 PiiType 열거형에 추가
enum class PiiType { ..., MY_NEW_TYPE };

// initPatterns()에 패턴 추가
PatternDef def;
def.type       = PiiType::MY_NEW_TYPE;
def.typeName   = L"새유형";
def.pattern    = std::wregex(LR"(\b\d{6}\b)", std::regex::optimize);
def.enabled    = true;
def.doValidate = false;
m_patterns.push_back(def);
```

검증 로직이 필요하면 `detect()` 내부의 validate 분기에 추가:

```cpp
if (pd.doValidate && pd.type == PiiType::MY_NEW_TYPE) {
    if (!validateMyNewType(matched)) continue;
}
```

---

## 7. 출력 파일 형식

### Excel (3시트)

```
시트1: 요약
  A1: 스캔 경로   B1: (경로값)
  A2: 스캔 일시   B2: (일시값)
  ...
  A8: [유형별 탐지 건수] 제목
  A9: 유형  B9: 건수
  ...

시트2: 파일 목록
  헤더: 파일경로 | 확장자 | 탐지건수 | 추출방법 | 소요(초) | 오류
  AutoFilter 적용

시트3: 상세 결과
  헤더: 파일경로 | 탐지유형 | 탐지값 | 마스킹값 | 줄번호 | 맥락 | 신뢰도
  AutoFilter 적용
```

### HTML

- 단일 `.html` 파일 (외부 의존 없음)
- 3개 탭: 유형별 통계 차트 / 파일 목록 / 상세 결과
- UTF-8 BOM 포함 (Excel에서 직접 열 때 한글 깨짐 방지)
- `htmlEscape()`로 모든 사용자 데이터 이스케이프 적용

---

## 8. 확장 포인트

| 확장 목표 | 방법 |
|----------|------|
| 새 PII 유형 추가 | `PiiType` 열거형 + `initPatterns()` + `getTypeName()` |
| 새 파일 형식 지원 | `TextExtractor::extract()` 분기 추가 |
| 임시 폴더 정리 | `DeleteDirectoryRecursive()` (Win32) — `_wsystem`/`cmd.exe` 미사용 |
| 쿼리 폴백 스캐너 | `EverythingScanner` 실패 시 `FindFirstFileW` 기반 폴백 |
| JSON 출력 | `Reporter::saveJson()` 추가 |
| 증분 스캔 | Everything `date_modified:>2026-01-01` 필터 활용 |
| GUI | WTL 또는 Win32 직접, 또는 CLI를 C# WinForms wrapper로 감싸기 |
