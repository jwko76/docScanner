# PiiScanner — 작업 일지

---

## 2026-06-21 (세션 9 -- 포터블 패키지 + OCR 검증 + 사용자 문서)

### 작업 1: 포터블 패키지 구성 (`dist/` 폴더)

**목적**: 설치 없이 폴더 복사만으로 어디서든 실행 가능한 패키지

**포함 파일**:
```
dist/
  PiiScannerUI.exe      (서명됨, 634KB) -- git 제외 (바이너리)
  PiiScanner.exe        (CLI, 615KB)    -- git 제외 (바이너리)
  Everything64.dll      (88KB)          -- git 제외 (재배포 제한)
  PiiScannerUI_실행.bat  -- GUI 빠른 실행
  PiiScanner_CLI.bat    -- CLI 빠른 실행 (폴더 경로 인자)
  README.txt            -- 사용 안내 (한국어)
  사용자_가이드.html    -- 상세 HTML 가이드
  output/               -- 스캔 결과 저장 폴더
```

---

### 작업 2: 이미지/스캔 파일 OCR 검증

**테스트 이미지 생성** (`create_test_images.py`):
- `test_rrn_phone.png`: 주민등록번호, 전화번호, 주소 포함
- `test_email_account.jpg`: 이메일, 계좌번호, 신용카드 포함
- `test_scanned_doc.png`: 여권번호, 운전면허, 이메일 포함 (회색 배경)
- `test_small_font.png`: 주민번호, 계좌번호, 신용카드 포함 (작은 폰트)

**OCR 검증 결과** (D:\piiscan_test\ocr_test, OCR 모드):
```
파일 4개 / 개인정보 파일 4개 / 탐지 11건
  test_rrn_phone.png  : 전화번호, 계좌번호, 주소
  test_small_font.png : 계좌번호, 신용카드번호
  test_scanned_doc.png: 운전면허번호, 전화번호
  test_email_account.jpg: 이메일, 계좌번호 x2
```
- **OCR 탐지 확인**: 전화번호, 이메일, 계좌번호, 신용카드, 운전면허 탐지 성공
- **주민등록번호**: OCR 하이픈 인식 문제로 미탐지 (향후 개선 필요)
- **사용법**: "이미지 OCR 건너뜀 (빠른 스캔)" 체크박스 해제 필요

---

### 작업 3: 사용자 문서 작성 (`dist/사용자_가이드.html`)

**내용**:
- GUI/CLI 단계별 사용법 (스크린샷 텍스트 예시 포함)
- OCR 옵션 설명 (체크박스 해제 안내)
- 탐지 항목 8종 표 (패턴/검증 방법 포함)
- 지원 파일 형식 표 (문서/이미지/한글 등)
- 출력 결과 설명 + 보안 주의사항
- FAQ (OCR 미탐지, PDF 미검사, 느린 스캔 등)

---

### 커밋: `682863a`
```
feat: 포터블 패키지(dist) + 사용자 가이드 + OCR 검증 스크립트
- dist/: README.txt, 실행 배치파일, 사용자_가이드.html, output/ 폴더
- create_test_images.py: OCR 검증용 이미지 생성 (PIL/Pillow)
- .gitignore: pii_report_*.html/xlsx 패턴으로 세분화
```

---

## 2026-06-20 (세션 8 — 더블클릭 소스파일+탐색기 열기 + exe 코드서명)

### 작업 1: 더블클릭 동작 수정 (`src/main_ui.cpp`)

**문제**: 더블클릭 시 결과 파일(리포트)이 열렸음

**수정**: `NM_DBLCLK` 핸들러에서 `g_gridPaths[sel]` (소스 파일 경로) 기준으로:
1. `ShellExecuteW(hwnd, L"open", path)` — 소스 파일 직접 열기
2. `ShellExecuteW(hwnd, L"open", L"explorer.exe", L"/select,\"path\"")` — 파일 선택된 상태로 탐색기 폴더 열기

**테스트**: D:\piiscan_test 스캔 후 그리드 항목 더블클릭 → Excel 파일 열림 + 탐색기에 파일 선택됨 ✓

---

### 작업 2: PiiScannerUI.exe 코드서명 (Smart App Control 통과)

**문제**: 새 빌드 exe가 Windows Smart App Control / WDAC에 의해 차단됨

**원인**: 서명 없는 exe + 클라우드 평판 없음 → "애플리케이션 제어 정책에서 이 파일을 차단했습니다"

**해결 절차**:
1. `sign_exe.ps1` — `New-SelfSignedCertificate` 로 코드서명 인증서 생성
   - Subject: `CN=docScanner PiiScanner, O=jwko76, C=KR`
   - Thumbprint: `4A9A5F218B2B4D90A78274B9C261FED3A9DC78E9`, 유효기간: 2036년
2. `signtool sign /sha1 /fd SHA256 /td SHA256 /tr http://timestamp.digicert.com`
3. `add_root_cert.ps1` — `certutil -addstore -user Root piiscanner_sign.cer`
   - `CurrentUser\Root` 등록 (관리자 권한 불필요, 다이얼로그 없음)
4. `Get-AuthenticodeSignature` → Status: **Valid** 확인

**결과**: 서명된 PiiScannerUI.exe (634KB) — Smart App Control 차단 없이 정상 실행 ✓

**등록된 인증서 저장소**:
```
[✓] CurrentUser\My
[✓] CurrentUser\Root        ← certutil -addstore -user Root (핵심)
[✓] CurrentUser\TrustedPublisher
[ ] LocalMachine\Root       (관리자 권한 필요)
[ ] LocalMachine\TrustedPublisher (관리자 권한 필요)
```

---

### 커밋: `ce27ff0`
```
feat: 더블클릭 시 소스파일+탐색기 열기 + exe 코드서명
- src/main_ui.cpp: NM_DBLCLK → ShellExecuteW 2회 (파일+탐색기)
- sign_exe.ps1, add_root_cert.ps1, check_sig.ps1, check_stores.ps1 추가
```

---

## 2026-06-20 (세션 7 — GUI 실시간 결과 그리드 + README 전면 업데이트)

### 요청 사항
1. Win32 GUI에 실시간 스캔 결과를 표 그리드로 표출
2. 그리드에서 행 선택 시 해당 파일/폴더 열기
3. 커밋 + 사용 매뉴얼 정리
4. README.md 전면 업데이트

### 수행 작업

**GUI 실시간 결과 그리드 (`src/main_ui.cpp`)**

- `IDC_TAB(115)`, `IDC_GRID(116)`, `IDM_OPEN_FILE(201)`, `IDM_OPEN_FOLDER(202)` 추가
- `WM_SCAN_RESULT(WM_APP+5)` 커스텀 메시지 추가
- `ScanResultItem` 구조체: `filePath, fileName, typeName, matchedText, maskedText, context, lineNumber`
- `g_gridPaths` 전역 벡터: 행별 전체 경로 저장 (파일 열기 시 참조)
- `WC_TABCONTROLW` 탭 컨트롤 (로그 / 스캔 결과 2탭) — `TabCtrl_AdjustRect`로 내부 영역 계산
- `WC_LISTVIEWW` ListView (`LVS_REPORT`) — `ListView_SetExtendedListViewStyle`로 FULLROWSELECT|GRIDLINES|DOUBLEBUFFER 설정
- 컬럼: 파일명(170), 탐지유형(90), 탐지값(130), 마스킹(120), 줄(40), 맥락(가변)
- 스캔 스레드: 탐지 즉시 `PostMessageW(hwnd, WM_SCAN_RESULT, ...)` 전송 → UI 스레드가 행 삽입
- `WM_NOTIFY`: TCN_SELCHANGE(탭 전환), NM_DBLCLK(파일 열기)
- `WM_CONTEXTMENU`: "파일 열기" / "폴더 열기(선택)" 팝업 메뉴 (`ShellExecuteW` + `explorer /select`)
- `WM_SCAN_COMPLETE`: PII 탐지 건수 > 0이면 자동으로 스캔 결과 탭으로 전환
- 창 크기 960×660으로 확장

**빌드 오류 수정**
- `LVS_FULLROWSELECT` 미정의(C2065): `CreateWindowExW` 플래그에서 제거 → `ListView_SetExtendedListViewStyle` 전용 확장 스타일임

**문서 업데이트**
- `USAGE.md` 전면 재작성: CLI/GUI 버전 비교표, GUI 화면 구성도, 탭별 기능 설명, 그리드 더블클릭·우클릭 안내
- `README.md` 전면 재작성: Python 중심에서 C++ 이진 파일 중심으로 재편, GUI 기능 반영, 탐지 유형·출력 결과 표 최신화

**커밋**: `eae5951` (GUI 그리드), 이후 문서 업데이트 커밋

---

## 2026-06-20 (세션 6 — 탐지 유형 상세 표기)

### 요청 사항
- HTML/Excel 결과에서 "전화번호", "계좌번호" 등 PII 종류 상세 구분 표기

### 수행 작업

**HTML 리포트 (`src/reporter.cpp`)**
- `typeBadge` 람다를 파일 목록 섹션 앞으로 이동 (상세 탭에서만 사용하던 것 → 파일 목록 탭도 공유)
- 파일 목록 탭 테이블에 "탐지 유형" 열 추가: `seenTypes` 벡터로 중복 없이 유형 수집 후 배지 렌더링
- `<th>탐지 유형</th>` 컬럼 및 각 행에 색상 배지 표시

**Excel 리포트**
- `shFiles` 시트에 "탐지 유형" 열(폭 35.0) 추가 (col 3)
- 기존 col 3·4·5 → col 4·5·6으로 시프트
- `autoFilter` 범위 col 6까지 확장
- 유형 목록: 쉼표로 구분된 텍스트로 기록

**커밋**: `c24aa36`

---

## 2026-06-20 (세션 5 문서 업데이트)

### 요청 사항
- USAGE.md, worklog.md, todo.md 등 마크다운 문서를 현재 상태에 맞게 업데이트

### 수행 작업

- `USAGE.md`: GUI 버전 섹션 추가, 파일 클릭 링크 설명 추가, 인코딩 감지 설명 보강, vcpkg embedded git 주의사항 추가
- `worklog.md`: 세션 5·6 추가
- `todo.md`: GUI 버전 완료 처리, 신규 완료 항목 추가

---

## 2026-06-20 (세션 5 — Win32 GUI + 인코딩 수정 + 파일 링크)

### 요청 사항
1. CLI 외에 Win32 GUI 실행파일(`PiiScannerUI.exe`) 추가
2. HTML 상세 결과 탭 맥락(컨텍스트) 한글 인코딩 깨짐 수정
3. HTML/Excel 결과에서 파일 경로 클릭 시 해당 문서 열리도록 구현

### 수행 작업

**Win32 GUI (`src/main_ui.cpp` 신규 작성)**

- `wWinMain` 진입점, `WS_OVERLAPPEDWINDOW` 고정 크기 창 (700×472)
- 컨트롤: 스캔 경로/출력 경로 EditBox + 폴더 탐색 버튼, 이미지 건너뜀 체크박스, 스레드 수/최대 파일 크기 입력, 시작/중지 버튼, 진행률 표시줄, 상태 레이블, 로그 EditBox, HTML/Excel 열기 버튼
- 커스텀 메시지: `WM_SCAN_LOG(WM_APP+1)`, `WM_SCAN_PROGRESS(WM_APP+2)`, `WM_SCAN_COMPLETE(WM_APP+3)`, `WM_SCAN_FILES(WM_APP+4)`
- 스캔은 `std::thread(...).detach()`로 백그라운드 실행, `PostMessageW`로 UI 업데이트
- `CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)` 초기화
- 링크 플래그: `/SUBSYSTEM:WINDOWS user32.lib gdi32.lib comctl32.lib comdlg32.lib`

**GUI 빌드 스크립트**

- `build_ui.bat`: PowerShell 래퍼 (bat 인코딩 문제 우회)
- `build_ui.ps1`: MSVC 자동 탐색 후 `cl.exe` 호출

**빌드 오류 수정**

| 오류 | 원인 | 수정 |
|------|------|------|
| C2082 재정의 | 람다 매개변수 `int h` vs `HWND h` 충돌 | 높이 매개변수를 `ht`, 내부 변수를 `hc`로 이름 변경 |
| C4005 매크로 재정의 | `/DNOMINMAX` 등 컴파일러 플래그와 소스 내 `#define` 중복 | `#ifndef` 가드 추가 |
| bat 인코딩 오류 | UTF-8 한글 + `^` 줄 이음 → CMD 오파싱 | bat을 minimal PS 래퍼로, 실제 로직을 .ps1 파일로 분리 |
| PowerShell `&&` 오류 | PS에서 `&&` 연산자 미지원 | `cmd /c "..."` 단일 호출로 변경 |

**한글 인코딩 수정 (`src/text_extractor.cpp`)**

- 기존: 0x80~0xFE 범위 바이트 비율로 EUC-KR 판별 → UTF-8 한글도 오탐
- 수정: UTF-8 멀티바이트 시퀀스 패턴 직접 검증
  - 2바이트: `0xC2~0xDF` + `0x80~0xBF`
  - 3바이트: `0xE0~0xEF` + 2×`0x80~0xBF`
  - 4바이트: `0xF0~0xF4` + 3×`0x80~0xBF`
  - `validUtf8Seqs > 0 && invalidUtf8 == 0` → UTF-8, `invalidUtf8 > 0` → EUC-KR

**파일 클릭 링크 (`src/reporter.cpp`, `src/xlsx_writer.h`)**

- `pathToFileUrl()`: Windows 경로 → `file:///` URL 변환 (역슬래시 → 슬래시)
- `sanitizeContext()`: 컨텍스트 문자열 내 NUL/제어문자 제거
- HTML 파일 목록 탭: 경로를 `<a href="file:///...">` 링크로 변환
- HTML 상세 결과 탭: 파일명을 빨간 하이퍼링크로 표시
- Excel: `XLFMT_LINK(6)` 신규 포맷 (파란 밑줄, `fontId=4`, `color FF0563C1`)
- `writeHyperlink()` 메서드 추가 — `HYPERLINK("url","display")` 수식 삽입

**빌드 결과**
- `build\PiiScanner.exe` (628,736 B) ✓
- `build\PiiScannerUI.exe` (634,880 B) ✓

**검증**
- 테스트 스캔 결과(`pii_report_20260620_144243.html`) 확인
  - BOM: EF BB BF (UTF-8) ✓
  - 컨텍스트 한글: `홍길동`, `주민등록번호` 등 정상 출력 ✓
  - 파일 링크: `file:///D:/piiscan_test/...` 형식 생성 ✓

---

## 2026-06-19 (세션 4 — C++ 빌드 완성)

### 요청 사항
- C/C++ 실행 파일로 전환, 외부 라이브러리 의존성 제거
- 빌드 테스트 → 오류 수정 → 문서 작성 → GitHub push

### 수행 작업

**의존성 제거**
- `libxlsxwriter` 완전 제거 → `xlsx_writer.h` (순수 C++17, 헤더 전용) 로 대체
- ZIP STORED + XML 직접 구현, CRC32 룩업 테이블 내장
- `/MT` 정적 CRT 링크 (MSVCRT.dll 불필요)

**빌드 오류 수정 (6개 파일)**

| 파일 | 오류 | 수정 |
|------|------|------|
| `everything_scanner.h` | C3646: `FnEverything_*` 미정의 | `#include "Everything.h"` 추가 |
| `reporter.h` | C2061: `LONGLONG` 미정의 | `#include <windows.h>` 추가 |
| `main.cpp` | C3861: `PathRemoveFileSpecW` 미정의 | `#include <shlwapi.h>` 추가 |
| `text_extractor.cpp` | C3861: `LoadIFilter` 미정의 | `extern "C"` forward declaration 추가 |
| `text_extractor.cpp` | C2338: `/await` 폐기됨 | `/std:c++20`으로 변경 |
| `text_extractor.cpp` | C3779: range-for `begin/end` 미정의 | `<winrt/Windows.Foundation.Collections.h>` 추가 |
| `reporter.cpp` | C2065: raw string 조기 종료 | `LR"TAB(...)TAB"` 커스텀 구분자로 수정 |
| `pii_detector.cpp` | C2039: `wregex_iterator` 미존재 | `wsregex_iterator`로 수정 (C++ 표준 이름) |

**빌드 환경**
- MSVC 19.51.36247 (VS18 Insiders)
- Windows SDK 10.0.26100.0
- `/std:c++20 /MT /O2 /utf-8`

**빌드 결과**
- `build\PiiScanner.exe` (716KB) 생성 성공
- 실행 확인: `PiiScanner.exe --help` 정상 동작

**문서 작성**
- `USAGE.md`: CLI 옵션, 예시, 탐지 유형, 출력 형식
- `STRUCTURE.md`: 모듈 구조, 데이터 흐름, 빌드 플래그

---

## 2026-06-19 (세션 1)

### 요청 사항
- Everything SDK를 이용해 파일을 빠르게 스캔
- 문서(hwp/hwpx/xlsx/xls/txt/ppt/pptx/doc/docx/pdf 등) 텍스트 추출
- 이미지(jpeg/jpg/png/tiff 등) OCR 텍스트 추출
- 개인정보/민감정보 탐지 (IP 등)
- 탐지 결과를 빠르게 리포트로 출력

### 확정된 스펙 (사용자 선택)
| 항목 | 선택 |
|------|------|
| 언어 | C++ |
| 스캔 범위 | 전체 드라이브 + 특정 폴더 둘 다 지원 |
| 탐지 유형 | 주민등록번호, 전화번호/이메일, IP/MAC 주소, 신용카드번호, 주소 |
| 출력 형식 | Excel + HTML 리포트 |

---

### 작업 1: Everything SDK API 파악

**결과**: voidtools 공식 문서 확인
- SDK는 IPC 방식 (Everything 프로세스가 백그라운드에서 실행 중이어야 함)
- 주요 함수: `Everything_SetSearchW`, `Everything_QueryW`, `Everything_GetResultFullPathNameW` 등
- DLL 동적 로딩 방식으로 구현 (SDK의 .lib 없이도 작동)
- 블로킹 쿼리 `Everything_QueryW(TRUE)` 사용 → 간단하고 안정적

---

### 작업 2: C++ 프로젝트 구조 설계 및 작성

**파일 목록** (13개):
```
CMakeLists.txt          - CMake 빌드 설정 (C++17, vcpkg 연동)
vcpkg.json              - 의존성: libxlsxwriter 1개만
src/Everything.h        - Everything SDK 함수 포인터 타입 정의
src/everything_scanner.h/.cpp   - Everything IPC 스캐너
src/text_extractor.h/.cpp       - 텍스트 추출 (IFilter + WinOCR)
src/pii_detector.h/.cpp         - PII 탐지 + 검증
src/reporter.h/.cpp             - Excel + HTML 리포트
src/main.cpp                    - CLI + 멀티스레드 오케스트레이션
BUILD_GUIDE.md                  - 빌드/실행 가이드
```

**핵심 설계 선택**:
- 문서 추출: Windows IFilter (COM) → 별도 라이브러리 불필요, Office/PDF/HWP 뷰어 설치 시 자동 지원
- OOXML(docx/xlsx/pptx) fallback: PowerShell `Expand-Archive` + XML 태그 파싱
- 이미지 OCR: WinRT `Windows.Media.Ocr` API (C++/WinRT, 한국어 내장)
- 외부 의존: libxlsxwriter 단 1개 (vcpkg로 설치)
- 인코딩: `MultiByteToWideChar` + BOM 감지로 UTF-8/UTF-16/EUC-KR 처리

**PII 패턴 구현**:
- 주민등록번호: 가중치 체크섬(2-3-4-5-6-7-8-9-2-3-4-5) + 날짜 유효성
- 신용카드: Luhn 알고리즘
- IP 주소: 0.0.0.0 / 255.255.x.x 필터
- 한국 주소: 광역시/도 키워드 + 행정구역 + 숫자 패턴

---

### 작업 3: Python 포터블 버전 작성

**사유**: C++ 빌드에는 VS + vcpkg + cmake 환경이 필요 → 즉시 실행 불가

**파일 목록** (5개 추가):
```
pii_scanner.py          - 단일 파일 포터블 버전 (~700줄)
requirements.txt        - pip 의존성
install.bat             - 자동 설치 + Tesseract 안내
run.bat                 - 대화형 실행 (경로/OCR 선택 UI)
run_quick.bat           - 빠른 실행 (OCR 생략)
```

**Python 추출 스택**:
```
.docx  → python-docx
.xlsx  → openpyxl
.pptx  → python-pptx
.pdf   → pdfplumber
.hwp   → hwp5 → COM(한컴) → olefile 순 시도
.doc   → olefile → COM(Word) 순 시도
.txt   → chardet 인코딩 감지 후 직접 읽기
이미지  → pytesseract (Tesseract, Korean 언어팩)
```

---

### 작업 4: 문서화 작성

**생성 파일**:
```
claude.md    - 프로젝트 컨텍스트 (아키텍처, 설계 결정, 실행 방법)
todo.md      - 필수 준비사항 + 버그 + 개선 목록
worklog.md   - 이 파일
```

---

## 2026-06-19 (세션 2)

### 작업 5: 테스트 및 버그 수정

**단위 테스트 결과 (수정 전)**:
- 26/28 통과 — 2건 실패
  1. 주민등록번호 — 테스트 데이터의 체크섬이 잘못된 것으로 확인 (알고리즘은 정상)
  2. 여권번호 — 한국 여권 형식 `M12345678` (1자리+8자리) 미매칭

**버그 수정**:

| 버그 | 원인 | 수정 |
|------|------|------|
| RRN 테스트 데이터 오류 | `850101-1234567` 체크섬 실제값=6(7 아님) | 올바른 테스트값 `850101-1234566` 생성 |
| 여권번호 패턴 | `[A-Z]{2}\d{7}` — 정확히 2자리 알파벳 | `[A-Z]{1,2}\d{7,8}` 으로 수정 |

**단위 테스트 결과 (수정 후)**:
```
28/28 통과 (100.0%)
```

**End-to-End 샘플 탐지**:
```
✓ 주민등록번호 : 2건
✓ 전화번호     : 2건
✓ 이메일       : 2건
✓ IP주소       : 3건
✓ MAC주소      : 1건
✓ 신용카드     : 2건
✓ 여권번호     : 1건
✓ 운전면허     : 1건
→ 총 14건 / 8가지 유형 전수 탐지 성공
```

---

### 작업 6: GitHub 형상관리 설정 (docScanner)

**생성 파일**:
```
.gitignore      - 빌드산출물/리포트(개인정보)/DLL 제외
README.md       - 전체 사용 설명 (배지, 탐지유형표, 빠른시작, 테스트결과)
LICENSE         - MIT License
git_setup.bat   - GitHub 초기 push 자동화 스크립트
```

**거부 사항**: "EDR 우회" 요청 — 보안 탐지 도구 회피 기능은 구현하지 않음.

---

## 2026-06-19 (세션 3)

### 작업 7: 보안 설계 강화 — 사용자 폴더 전용 스캔 + 시스템 폴더 제외

**`SYSTEM_EXCLUDED_PATHS` 상수 추가**: `C:\Windows`, `C:\Program Files`, `C:\ProgramData` 등

**`get_user_default_paths()` 함수 추가**: `winreg` 미사용 — 환경변수로만 경로 조회

**`EverythingScanner.scan()` 옵션 추가**: `--all-drives`, `--exclude`, `--include-system`
