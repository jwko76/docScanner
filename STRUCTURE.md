# PiiScanner 코드 구조

## 디렉토리 구성

```
PiiScanner/
├── src/                        # C++ 소스 코드
│   ├── Everything.h            # Everything SDK 함수 포인터 typedef (voidtools)
│   ├── everything_scanner.h/cpp  # Everything IPC 파일 스캐너
│   ├── text_extractor.h/cpp    # 문서/이미지 텍스트 추출
│   ├── pii_detector.h/cpp      # 개인정보 패턴 탐지
│   ├── reporter.h/cpp          # Excel + HTML 리포트 생성
│   ├── xlsx_writer.h           # 순수 C++ xlsx 생성기 (의존성 없음)
│   └── main.cpp                # 진입점, CLI 파싱, 파이프라인 조율
├── sdk/                        # Everything64.dll 배치 위치
├── build/                      # 빌드 출력 (PiiScanner.exe, *.obj)
├── build.bat                   # MSVC 직접 빌드 스크립트
├── CMakeLists.txt              # CMake 빌드 정의
├── git_commit.bat              # git add + commit + push 헬퍼
├── USAGE.md                    # 사용자 가이드
├── STRUCTURE.md                # 이 파일
├── README.md                   # 프로젝트 소개
├── BUILD_GUIDE.md              # 상세 빌드 가이드
├── worklog.md                  # 개발 세션 기록
└── todo.md                     # 개선 항목 목록
```

---

## 모듈 구조 및 의존 관계

```
main.cpp
  ├── everything_scanner.h   (파일 목록 수집)
  ├── text_extractor.h       (텍스트 추출)
  ├── pii_detector.h         (개인정보 탐지)
  └── reporter.h             (리포트 저장)
         └── xlsx_writer.h   (Excel 생성, 내부용)
```

---

## 모듈 상세

### `everything_scanner.h / .cpp`

Everything SDK를 동적 로딩(LoadLibrary)하여 IPC 쿼리로 파일을 탐색합니다.

**주요 타입**

```cpp
struct FileEntry {
    std::wstring fullPath;
    std::wstring extension;   // 소문자 (예: L".docx")
    LONGLONG     fileSize;
    FILETIME     dateModified;
    bool         isDocument;
    bool         isImage;
};
```

**주요 메서드**

| 메서드 | 설명 |
|--------|------|
| `initialize(dllPath)` | Everything64.dll 로드, 연결 확인 |
| `scanFiles(rootPath, progress)` | rootPath 하위 파일 목록 반환 |
| `documentExtensions()` | 지원 문서 확장자 집합 (정적) |
| `imageExtensions()` | 지원 이미지 확장자 집합 (정적) |

**동작 흐름**

1. `LoadLibrary("Everything64.dll")` → `GetProcAddress`로 함수 포인터 획득
2. `Everything_SetSearchW(query)` → `Everything_SetRequestFlags(...)` → `Everything_QueryW(TRUE)`
3. 결과 순회: `Everything_GetResultFullPathNameW`, `Everything_GetResultSize`, `Everything_GetResultDateModified`
4. 소멸자: `Everything_CleanUp()` + `FreeLibrary()`

---

### `text_extractor.h / .cpp`

파일 확장자에 따라 최적 추출 방식을 선택하여 `wstring` 텍스트를 반환합니다.

**추출 전략 (우선순위)**

| 확장자 | 1차 | 2차 (fallback) |
|--------|-----|--------------|
| `.txt` `.log` `.csv` `.xml` ... | 직접 읽기 | — |
| `.docx` `.xlsx` `.pptx` | Windows IFilter | ZIP+XML 직접 파싱 |
| `.doc` `.xls` `.ppt` `.pdf` `.hwp` | Windows IFilter | — |
| `.jpg` `.png` `.tif` ... | Windows OCR API | — |

**인코딩 자동 감지**

BOM 검사 순서: UTF-16 LE → UTF-16 BE → UTF-8 BOM → UTF-8 heuristic → EUC-KR

**WinRT OCR 초기화**

```cpp
winrt::init_apartment(winrt::apartment_type::multi_threaded);
// 한국어 우선: OcrEngine::TryCreateFromLanguage(Language(L"ko-KR"))
// 없으면 영어: OcrEngine::TryCreateFromLanguage(Language(L"en-US"))
```

**주요 메서드**

| 메서드 | 설명 |
|--------|------|
| `extract(filePath, ext)` | 확장자 기반 자동 추출 |
| `extractPlainText(path)` | 텍스트 파일 직접 읽기 |
| `extractViaIFilter(path)` | COM IFilter 추출 |
| `extractOoxml(path, ext)` | ZIP 내 XML 파싱 |
| `extractViaWinOcr(path)` | WinRT OCR |

---

### `pii_detector.h / .cpp`

정규식 + 체크섬 검증으로 개인정보를 탐지합니다.

**탐지 패턴 초기화** (`initPatterns()`)

각 `PatternDef`에는 `PiiType`, `wregex`, `enabled`, `doValidate` 필드가 있습니다.

| 유형 | 정규식 예시 | 추가 검증 |
|------|------------|----------|
| `RESIDENT_NUMBER` | `\b\d{6}-[1-4]\d{6}\b` | 체크섬 + 날짜 |
| `CREDIT_CARD` | Luhn 패턴 | Luhn 알고리즘 |
| `IP_ADDRESS` | `\b\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}\b` | 사설/특수 주소 제외 |
| `PHONE_NUMBER` | `0\d{1,2}[-. ]\d{3,4}[-. ]\d{4}` | — |
| `EMAIL` | RFC 5322 기반 | — |
| `ADDRESS` | 광역시/도 + 구/동/로 키워드 | 별도 함수 |

**탐지 흐름**

```
detect(text)
  for each PatternDef:
    wsregex_iterator로 모든 매치 순회
    필요 시 validateResidentNumber / validateCreditCard / validateIpAddress 호출
    PiiMatch(type, matchedText, maskedText, position, lineNumber, contextSnippet, confidence) 생성
  detectAddress(text) 별도 호출
  return allMatches
```

**마스킹 예시**

- 주민등록번호: `850101-*******`
- 신용카드: `4532-****-****-5678`
- 전화번호: `010-****-5678`

---

### `reporter.h / .cpp`

스캔 결과를 Excel(.xlsx)과 HTML로 저장합니다.

**Excel 구조** (xlsx_writer.h 사용)

```
시트 1: 요약
  - 스캔 경로, 일시, 파일 수, 탐지 건수, 소요 시간
  - 유형별 탐지 건수 표

시트 2: 파일 목록
  - 파일 경로, 확장자, 탐지 건수, 추출 방법, 소요 시간, 오류
  - AutoFilter 적용

시트 3: 상세 결과
  - 파일 경로, PII 유형, 마스킹된 원문, 줄 번호, 신뢰도, 맥락
  - AutoFilter 적용
```

**HTML 구조**

- 인라인 CSS + vanilla JavaScript (외부 의존성 없음)
- 탭 전환: 유형별 통계 차트 / 파일 목록 / 상세 결과
- UTF-8 BOM으로 저장 (Excel에서 한글 깨짐 방지)

---

### `xlsx_writer.h`

순수 C++17 헤더 전용(header-only) xlsx 생성기. 외부 라이브러리 없음.

**구현 기술**

| 항목 | 방식 |
|------|------|
| 파일 포맷 | ZIP STORED (비압축) |
| CRC32 | 256-entry 룩업 테이블 (다항식 0xEDB88320) |
| XML 생성 | 직접 문자열 조합 |
| 한글 | UTF-8 인코딩 (OpenXML 규격) |

**미리 정의된 셀 형식**

```cpp
#define XLFMT_DEFAULT  0   // 기본
#define XLFMT_HEADER   1   // 굵은 흰색, 파란 배경, 테두리
#define XLFMT_CELL     2   // 테두리, 줄바꿈
#define XLFMT_CELL_RED 3   // 테두리, 굵은 빨간 글자
#define XLFMT_NUM      4   // 테두리, 우정렬 숫자
#define XLFMT_TITLE    5   // 굵은 14pt
```

**주요 메서드**

```cpp
XlsxWriter wb;
int sheet = wb.addWorksheet("시트명");
wb.setColumnWidth(sheet, col, width);
wb.writeString(sheet, row, col, "텍스트", XLFMT_HEADER);
wb.writeNumber(sheet, row, col, 42.0, XLFMT_NUM);
wb.setAutoFilter(sheet, r1, c1, r2, c2);
wb.save("output.xlsx");
```

---

### `main.cpp`

CLI 파싱과 전체 파이프라인을 조율합니다.

**파이프라인**

```
1. parseArgs()          CLI 인자 파싱
2. EverythingScanner    초기화 + scanFiles() → FileEntry 목록
3. 멀티스레드 풀        각 파일에 대해:
     TextExtractor::extract()
     PiiDetector::detect()
     결과를 results 벡터에 추가 (mutex 보호)
4. Reporter::saveAll()  xlsx + html 동시 저장
5. 요약 출력            콘솔에 탐지 건수/파일 수 표시
```

**스레드 처리**

- `std::thread` + `std::mutex` + `std::atomic<int>` 사용
- 기본 스레드 수: `std::thread::hardware_concurrency()`
- 진행률: `printProgress(done, total, piiFound)` (콘솔 단일 라인 갱신)

---

## 빌드 플래그

| 플래그 | 의미 |
|--------|------|
| `/std:c++20` | C++20 표준 (WinRT 코루틴 지원) |
| `/MT` | 정적 CRT 링크 (MSVCRT.dll 의존성 없음) |
| `/utf-8` | 소스 파일 + 실행 파일 문자셋 UTF-8 |
| `/EHsc` | C++ 예외 처리 |
| `/O2` | 최적화 |
| `/DUNICODE /D_UNICODE` | Unicode API 사용 |
| `/DNOMINMAX` | Windows min/max 매크로 비활성화 |
| `/DWIN32_LEAN_AND_MEAN` | 불필요한 Windows 헤더 제외 |

**링크 라이브러리**

| 라이브러리 | 용도 |
|-----------|------|
| `pathcch.lib` | `PathRemoveFileSpecW` 등 경로 처리 |
| `shlwapi.lib` | Shell API 유틸리티 |
| `ole32.lib` | COM 초기화 |
| `oleaut32.lib` | OLE Automation |
| `query.lib` | IFilter (`LoadIFilter`) |
| `windowsapp.lib` | WinRT (Windows.Media.Ocr 등) |
| `shell32.lib` | Shell 함수 |

---

## 데이터 흐름

```
[Everything SDK]
      │
      ▼
FileEntry[]  (fullPath, size, dateModified, isDocument, isImage)
      │
      ▼ (N threads)
[TextExtractor]  →  ExtractionResult (text, method, error)
      │
      ▼
[PiiDetector]    →  PiiMatch[] (type, matched, masked, pos, line, confidence)
      │
      ▼
FileScanResult[] (filePath, matches[], extractionSuccess, ...)
      │
      ├── [Reporter::saveExcel()]  →  pii_report_TIMESTAMP.xlsx
      └── [Reporter::saveHtml()]   →  pii_report_TIMESTAMP.html
```
