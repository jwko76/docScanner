# PiiScanner — 시큐어 코딩 검토 보고서

> 검토 대상: `src/` 하위 C++ 소스 6개 파일  
> 검토 기준: OWASP Top 10, CWE, Microsoft SDL  
> 상태: 발견된 취약점 **전수 수정 완료**

---

## 1. 검토 범위 및 방법

| 파일 | 검토 항목 |
|------|----------|
| `main.cpp` | CLI 입력 검증, 정수 파싱 예외 처리 |
| `everything_scanner.cpp` | DLL 로딩 경로 검증, 쿼리 인젝션 |
| `text_extractor.cpp` | 쉘 명령 인젝션, 무한 재귀, 임시 파일 |
| `pii_detector.cpp` | ReDoS(정규식 서비스 거부), 버퍼 안전성 |
| `reporter.cpp` | HTML 인젝션, 경로 순회 |
| `xlsx_writer.h` | XML 인젝션, 버퍼 오버플로 |

---

## 2. 발견 및 조치 요약

| # | 파일 | 분류 | 심각도 | CWE | 상태 |
|---|------|------|--------|-----|------|
| S-01 | `main.cpp` | 입력 검증 미흡 | 중 | CWE-20 | ✅ 수정 |
| S-02 | `main.cpp` | 정수 파싱 예외 미처리 | 중 | CWE-391 | ✅ 수정 |
| S-03 | `everything_scanner.cpp` | 임의 DLL 로딩 | 높음 | CWE-427 | ✅ 수정 |
| S-04 | `everything_scanner.cpp` | 쿼리 인젝션 | 중 | CWE-74 | ✅ 수정 |
| S-05 | `text_extractor.cpp` | 쉘 명령 인젝션 | 높음 | CWE-78 | ✅ 수정 |
| S-06 | `text_extractor.cpp` | 무한 재귀 (스택 오버플로) | 중 | CWE-674 | ✅ 수정 |
| S-07 | `pii_detector.cpp` | ReDoS | 중 | CWE-1333 | ✅ 수정 |


---

## 3. 상세 취약점 및 조치 내용

---

### S-01 / S-02 · `main.cpp` — CLI 정수 파싱 예외 미처리 / 범위 검증 없음

**취약 코드 (수정 전)**
```cpp
cfg.maxFileSize = std::stoll(argv[++i]) * 1024 * 1024;
cfg.numThreads  = std::stoi(argv[++i]);
```

**문제**
- `std::stoll` / `std::stoi` 는 비숫자 입력 시 `std::invalid_argument`,  
  오버플로 시 `std::out_of_range` 예외를 던짐.  
  try/catch 없이 예외가 전파되면 프로세스가 비정상 종료됨.
- `--max-size 0`, `--max-size -1`, `--threads 99999` 등 의도하지 않은 값에  
  대한 범위 검증이 없어 리소스 고갈(DoS) 가능.

**조치 (수정 후)**
```cpp
try {
    long long mb = std::stoll(argv[++i]);
    if (mb <= 0 || mb > 10240) {
        std::wcerr << L"[오류] --max-size 값은 1~10240(MB) 범위여야 합니다.\n";
        exit(1);
    }
    cfg.maxFileSize = mb * 1024 * 1024;
} catch (const std::exception&) {
    std::wcerr << L"[오류] --max-size 인자가 숫자가 아닙니다.\n";
    exit(1);
}
// --threads: 1~256 범위 강제
```

---

### S-03 · `everything_scanner.cpp` — 임의 DLL 로딩 (CWE-427)

**취약 코드 (수정 전)**
```cpp
m_hDll = LoadLibraryW(path.c_str());  // path = 사용자 공급 --dll 인자
```

**문제**
- `--dll` 옵션으로 임의 경로를 지정할 수 있어, 악의적 DLL 로딩 가능.
- 파일 존재 여부와 확장자를 검사하지 않음.

**조치**
```cpp
// 1) 파일 존재 확인
DWORD attr = GetFileAttributesW(path.c_str());
if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) {
    m_lastError = L"DLL 파일을 찾을 수 없습니다: " + path;
    return false;
}
// 2) 확장자 .dll만 허용
std::wstring ext = path.substr(path.rfind(L'.'));
std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
if (ext != L".dll") { m_lastError = ...; return false; }
// 검증 통과 후 LoadLibraryW 호출
```

**잔여 권고사항**
- 절대 경로만 허용하도록 `PathIsRelativeW` 추가 검사 고려.
- 알려진 서명(Authenticode) 검증 추가 고려 (`WinVerifyTrust`).

---

### S-04 · `everything_scanner.cpp` — Everything 쿼리 인젝션 (CWE-74)

**취약 코드 (수정 전)**
```cpp
oss << L"\"" << rootPath << L"\" ";  // rootPath = 사용자 공급 --path 인자
```

**문제**
- `rootPath`에 큰따옴표(`"`)나 파이프(`|`)가 포함되면 Everything 쿼리 문법이 깨짐.  
  예) `--path C:\Users" | ext:exe ` → 원본 쿼리 구조 변조.

**조치**
```cpp
std::wstring safePath = rootPath;
safePath.erase(std::remove(safePath.begin(), safePath.end(), L'"'), safePath.end());
safePath.erase(std::remove(safePath.begin(), safePath.end(), L'|'), safePath.end());
oss << L"\"" << safePath << L"\" ";
```

---

### S-05 · `text_extractor.cpp` — PowerShell 명령 인젝션 (CWE-78)

**취약 코드 (수정 전)**
```cpp
std::wstring psCmd =
    L"powershell ... \"Expand-Archive -LiteralPath '" + filePath + L"' ...\"";
```

**문제**
- `filePath`에 단따옴표(`'`)가 포함되면 PowerShell `-LiteralPath '...'` 인자 구조가 깨짐.  
  예) 파일명 `it's.docx` → PowerShell 파싱 오류 또는 인젝션.

**조치**
```cpp
// 단따옴표를 ''(두 개)로 치환: PowerShell 이스케이프 관례
auto escapePsSingleQuote = [](const std::wstring& s) -> std::wstring {
    std::wstring out;
    for (wchar_t c : s) {
        if (c == L'\'') out += L"''";
        else            out += c;
    }
    return out;
};
std::wstring safeFilePath  = escapePsSingleQuote(filePath);
std::wstring safeTmpFolder = escapePsSingleQuote(tmpFolder);
```

**참고**: `-LiteralPath` 사용은 기존부터 올바름 (`-Path`는 와일드카드 해석함).  
단따옴표 이스케이프로 인젝션 완전 차단.

---

### S-06 · `text_extractor.cpp` — `eucKrToWString` 무한 재귀 (CWE-674)

**취약 코드 (수정 전)**
```cpp
std::wstring TextExtractor::eucKrToWString(const std::vector<uint8_t>& bytes) {
    int wlen = MultiByteToWideChar(949, ...);
    if (wlen <= 0) {
        return toWString(bytes);  // ← toWString → detectEncoding → EUC_KR → eucKrToWString
    }
    ...
}
```

**문제**
- `eucKrToWString` 실패 시 `toWString(bytes)` 재호출.
- `toWString`은 내부에서 `detectEncoding`을 다시 호출 → 동일 바이트에 대해  
  `EUC_KR` 반환 → `eucKrToWString` 재호출 → **무한 재귀 / 스택 오버플로**.

**조치**
```cpp
if (wlen <= 0) {
    // 재귀 없이 UTF-8로 직접 재시도
    int wlen2 = MultiByteToWideChar(CP_UTF8, 0, ...);
    if (wlen2 <= 0) return {};
    std::wstring wstr2(wlen2, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, ..., wstr2.data(), wlen2);
    return wstr2;
}
```

---

### S-07 · `pii_detector.cpp` — 주소 정규식 ReDoS (CWE-1333)

**취약 코드 (수정 전)**
```cpp
LR"([\s\S]{0,5}?)"   // 개행 포함 모든 문자, 최소 횟수 반복
LR"([\s\S]{0,20}?\d+)"
LR"([\s\S]{0,10}?(동|호|층)?)"
```

**문제**
- `[\s\S]`는 개행(`\n`)을 포함하므로, 정규식 엔진이 여러 줄에 걸쳐 역추적.  
- `{0,N}?` (lazy) + 뒤따르는 필수 패턴의 조합은 병리적 입력에서  
  **O(N²) ~ O(2^N)** 역추적 발생 (ReDoS).
- 악의적으로 조작된 문서(예: 수천 개의 "서울서울서울..." 패턴)로 CPU 고갈 가능.

**조치**
```cpp
LR"([^\n]{0,5})"    // 한 줄 내에서만 매칭 → 역추적 범위 O(N)으로 제한
LR"([^\n]{0,20}\d+)"
LR"([^\n]{0,10}(동|호|층)?)"
```

`[^\n]`은 개행을 제외하므로 단일 라인 내에서만 매칭이 이루어져  
역추적이 줄 길이에 선형으로 제한됩니다.

---

## 4. 안전하게 구현된 항목 (양호)

| 항목 | 파일 | 설명 |
|------|------|------|
| HTML 이스케이프 | `reporter.cpp` | `htmlEscape()` — `<>&"'` 모두 처리 |
| XML 이스케이프 | `xlsx_writer.h` | `xe()` — 제어문자 제거 포함 |
| IFilter null 체크 | `text_extractor.cpp` | `GetProcAddress` 결과 전수 검증 |
| 최대 결과 수 제한 | `everything_scanner.cpp` | `m_SetMax(500000)` |
| 탐지 건수 제한 | `pii_detector.cpp` | `m_maxMatches = 10000` |
| 파일 크기 제한 | `text_extractor.cpp` | `readFileBytes(path, maxBytes)` |
| 스택 버퍼 경계 | `text_extractor.cpp` | `wchar_t buf[8192]; ULONG cchText = std::size(buf);` |
| 주민등록번호 체크섬 | `pii_detector.cpp` | 가중치 검증 + 날짜 유효성 |
| Luhn 알고리즘 | `pii_detector.cpp` | 신용카드 번호 정확도 향상 |
| 원자적 카운터 | `main.cpp` | `std::atomic<int>` — 스레드 안전 |
| 결과 mutex 보호 | `main.cpp` | `std::mutex` — data race 없음 |
| WinRT 예외 처리 | `text_extractor.cpp` | `winrt::hresult_error` catch |
| COM 반환값 검사 | `text_extractor.cpp` | `FAILED(hr)` 분기 처리 |
| 출력 경로 고정 | `reporter.cpp` | `baseName = "pii_report_" + timestamp` — 외부 입력 없음 |
| 레지스트리 미접근 | 전체 | `winreg` / `RegOpenKey` 미사용 |
| 네트워크 미접근 | 전체 | `socket` / `WinHttp` 미사용 |
| 정적 CRT 링크 | 빌드 | `/MT` — MSVCRT.dll 의존 없음 |

---

## 5. 보안 설계 원칙

### 최소 권한 원칙
- 프로세스는 일반 사용자 권한으로 실행 (UAC 상승 불필요).
- 기본 스캔 범위: `%USERPROFILE%` 하위 사용자 공간만.
- 시스템 폴더(`Windows`, `Program Files`, `ProgramData` 등) 기본 제외.

### 데이터 최소 수집
- 탐지 결과는 마스킹 처리 후 저장 (`850101-*******`).
- 원문(unmasked) 데이터는 메모리에만 존재하며, 리포트에는 마스킹값 기록.

### 안전한 임시 파일 처리
- OOXML 추출 임시폴더: `%TEMP%\PiiScanTmp_{PID}_{Tick}` — PID/틱 기반 유일 이름.
- 처리 완료 즉시 `rd /s /q`로 삭제.
- 작업 실패 시에도 finally 블록 없이 삭제 코드가 오류/성공 경로 모두에 존재.

### 외부 코드 실행 최소화
- PowerShell 호출: OOXML fallback 시에만 사용, 인자 이스케이프 적용.
- LoadLibrary: `Everything64.dll` 단독 — 파일 존재/확장자 검증 후 로드.

---

## 6. 잔여 권고사항 (향후 개선)

| 우선순위 | 항목 | 설명 |
|---------|------|------|
| 중 | Authenticode 서명 검증 | `LoadLibraryW` 전 `WinVerifyTrust`로 DLL 서명 확인 |
| 중 | `--path` 절대 경로 강제 | `PathIsRelativeW` 검사 추가 |
| 중 | OOXML 추출: PowerShell 제거 | Cabinet API 또는 minizip으로 대체하면 쉘 의존 제거 |
| 하 | 임시 파일 권한 강화 | `CreateDirectory` 시 DACL로 현재 사용자만 접근 허용 |
| 하 | 결과 파일 암호화 | 리포트(xlsx/html)에 비밀번호 보호 또는 DPAPI 암호화 |
| 하 | 실행 로그 | 스캔 시작/종료/오류를 이벤트 로그 또는 파일에 기록 |
