# PiiScanner — 작업 일지

---

## 2026-06-20 (세션 6 — 문서 업데이트)

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

**포터블 특징**:
- 의존 패키지를 `import` 시점에 자동 설치 (`pip install --quiet`)
- 실행 환경: Python 3.8+ + pip만 있으면 됨
- OCR: Tesseract 미설치 시 이미지 파일 건너뜀 (오류 없음)
- HWP/COM: 해당 소프트웨어 미설치 시 자동 건너뜀

---

### 작업 4: 문서화 작성

**생성 파일**:
```
claude.md    - 프로젝트 컨텍스트 (아키텍처, 설계 결정, 실행 방법)
todo.md      - 필수 준비사항 + 버그 + 개선 목록
worklog.md   - 이 파일
```

---

### 최종 파일 트리

```
PiiScanner/
├── pii_scanner.py          ★ 즉시 실행 가능 (Python 포터블)
├── requirements.txt
├── install.bat
├── run.bat
├── run_quick.bat
│
├── src/                    C++ 고성능 버전
│   ├── Everything.h
│   ├── everything_scanner.h / .cpp
│   ├── text_extractor.h / .cpp
│   ├── pii_detector.h / .cpp
│   ├── reporter.h / .cpp
│   └── main.cpp
│
├── CMakeLists.txt
├── vcpkg.json
├── sdk/                    ← Everything64.dll 여기에 복사
│
├── claude.md
├── todo.md
└── worklog.md
```

---

### 즉시 실행 순서

```
1. install.bat 실행           → Python 패키지 자동 설치
2. Everything64.dll 복사      → SDK zip에서 추출
3. Everything 실행            → 백그라운드 상태로
4. run.bat 실행               → 스캔 경로/옵션 선택 후 시작
```

---

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

**사용자 진행 순서**:
1. https://github.com/new 에서 `docScanner` 리포지토리 생성
2. `PiiScanner/` 폴더에서 `git_setup.bat` 실행
3. GitHub 사용자명 + 이메일 입력 → 자동 init/commit/push

**미결 이슈**:
- [ ] HWP 바이너리 파싱 안정화
- [ ] 계좌번호 패턴 오탐률 감소
- [ ] PDF 스캔 이미지 → OCR fallback
- [ ] C++ 빌드 자동화

---

## 2026-06-19 (세션 3)

### 작업 7: 보안 설계 강화 — 사용자 폴더 전용 스캔 + 시스템 폴더 제외

**변경 배경**: 레지스트리 미접근, 비정상 네트워크 접근 없음, 사용자 공간만 검색하도록 설계 명확화

#### 변경 내용 (pii_scanner.py)

**① 독스트링 보안 설계 명시**
- `레지스트리 미접근 (winreg 사용 없음)` 명시
- `스캔 중 네트워크 미접근` 명시
- `--all-drives` 사용 시에도 시스템 폴더 제외 유지

**② `SYSTEM_EXCLUDED_PATHS` 상수 추가**
```
C:\Windows, C:\Windows.old
C:\Program Files, C:\Program Files (x86)
C:\ProgramData
C:\Recovery, C:\$Recycle.Bin, C:\System Volume Information
C:\Boot, C:\$WINDOWS.~BT
C:\Windows\SysWOW64, C:\Windows\System32
C:\ProgramData\Microsoft\Windows\Hyper-V
```

**③ `_is_excluded(path)` 함수 추가**
- `SYSTEM_EXCLUDED_PATHS` 기반 경로 접두사 비교 (대소문자 무시)

**④ `get_user_default_paths()` 함수 추가**
- `winreg` 미사용 — 환경변수(`%USERPROFILE%`, `%APPDATA%`, `%LOCALAPPDATA%`)로만 경로 조회
- OneDrive 경로도 환경변수로 탐지 (`OneDrive`, `OneDriveConsumer`, `OneDriveCommercial`)

**⑤ `EverythingScanner.scan()` 시그니처 변경**
```python
# Before:  scan(root_path: str = "")
# After:   scan(root_paths: List[str], exclude_system: bool = True, extra_excludes: List[str] = None)
```
- 결과 순회 시 `_is_excluded()` 및 `extra_excludes` 필터 적용
- 제외된 파일 수 콘솔 출력: `(시스템 폴더 제외: N개 건너뜀)`

**⑥ `parse_args()` 옵션 추가**
| 옵션 | 설명 |
|------|------|
| `--all-drives` | 전체 드라이브 스캔 opt-in (시스템 폴더 제외는 유지) |
| `--exclude 경로` | 추가 제외 폴더 (여러 번 사용 가능) |
| `--include-system` | 시스템 폴더 제외 비활성화 (비권장) |

**⑦ `main()` 기본 스캔 경로 변경**
- 기존: `""` (전체 드라이브)
- 변경: `get_user_default_paths()` 반환값 → `%USERPROFILE%` + `%APPDATA%` + `%LOCALAPPDATA%` + OneDrive

**⑧ 코드 검증 결과**
```
grep winreg / socket / urllib / requests → 본문 내 미발견
(주석·출력 문자열의 URL 문자열만 존재)
```

#### 변경 내용 (run.bat)
- 스캔 범위 선택 메뉴 (1=사용자폴더 / 2=특정폴더 / 3=전체드라이브)
- 보안 설계 안내 문구 상단에 표시
- `--exclude` 추가 제외 경로 입력 옵션

#### 변경 내용 (run_quick.bat)
- 보안 안내 문구 추가
- `--skip-images` 기본 유지 (사용자 폴더 대상)
