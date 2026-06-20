# docScanner

> 개인정보 보호법(PIPA) · GDPR 준수를 위한 문서 내 개인정보/민감정보 탐지 도구

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue)](https://isocpp.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-green)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%2B-lightgrey)](https://microsoft.com)

---

## 개요

로컬 컴퓨터에 저장된 파일을 빠르게 스캔하여 개인정보 및 민감정보가 포함된 파일을 탐지합니다.  
[Everything](https://www.voidtools.com/) SDK를 이용해 수십만 파일도 수초 내에 목록화하고,  
다양한 문서·이미지 형식에서 텍스트를 추출하여 정규식 + 체크섬 알고리즘으로 PII를 탐지합니다.

---

## 주요 기능

| 기능 | 설명 |
|------|------|
| **고속 파일 스캔** | Everything SDK IPC로 50만 파일도 수초 내 목록화 |
| **GUI + CLI 지원** | Win32 GUI(`PiiScannerUI.exe`) + 명령줄(`PiiScanner.exe`) |
| **실시간 결과 그리드** | 탐지 즉시 창에 행 추가, 더블클릭·우클릭으로 파일/폴더 열기 |
| **다양한 문서 지원** | hwp/hwpx · doc/docx · xls/xlsx · ppt/pptx · pdf · txt · csv 등 |
| **이미지 OCR** | Windows OCR API(WinRT)로 이미지 텍스트 추출 (한국어 우선) |
| **PII 탐지** | 주민번호·전화번호·이메일·IP/MAC·신용카드·계좌번호·주소 등 10가지 |
| **검증 알고리즘** | 주민번호 체크섬, 신용카드 Luhn, IP 유효성으로 오탐 최소화 |
| **멀티스레드** | CPU 코어 수에 맞춰 병렬 스캔 |
| **이중 리포트** | Excel 3시트 + 대화형 HTML (파일 클릭 시 문서 바로 열기, 탐지 유형 배지) |
| **의존성 제로** | 순수 C++20, 정적 CRT (`/MT`) — MSVCRT.dll 불필요 |

---

## 탐지 유형

| 유형 | 예시 | 검증 방식 |
|------|------|-----------| 
| 주민등록번호 | `850101-1234566` | 체크섬 + 날짜 유효성 ✓ |
| 전화번호 | `010-1234-5678` | 국내 국번 패턴 |
| 이메일 | `hong@company.co.kr` | RFC 패턴 |
| IP 주소 | `192.168.1.100` | 유효 범위 필터 ✓ |
| MAC 주소 | `AA:BB:CC:DD:EE:FF` | 패턴 |
| 신용카드 | `4532-0151-1283-0366` | Luhn 알고리즘 ✓ |
| 계좌번호 | `110-123-456789` | 은행 자릿수 패턴 |
| 주소 | `서울시 강남구 테헤란로 123` | 광역시/도 키워드 |
| 여권번호 | `M12345678` | 한국 여권 패턴 |
| 운전면허 | `11-20-123456-78` | 지역코드 패턴 |

---

## 요구사항

- **Windows 10** 이상 (WinRT OCR 필요)
- **[Everything](https://www.voidtools.com/)** 실행 중 권장 (없으면 파일시스템 직접 탐색으로 대체)
- **Everything SDK** — `Everything64.dll` ([다운로드](https://www.voidtools.com/support/everything/sdk/))

---

## 빠른 시작

### 1. 빌드

```bat
git clone https://github.com/jwko76/docScanner.git
cd docScanner

build.bat      ← CLI 버전 (PiiScanner.exe)
build_ui.bat   ← GUI 버전 (PiiScannerUI.exe)
```

MSVC(Visual Studio 2019 이상)가 설치된 환경에서 한 번에 빌드됩니다.

### 2. Everything64.dll 배치

```
build/
  PiiScanner.exe
  PiiScannerUI.exe
  Everything64.dll   ← SDK zip에서 여기에 복사
```

### 3. 실행 (GUI — 권장)

```bat
build\PiiScannerUI.exe
```

1. 스캔 경로 입력 → `▶ 스캔 시작`
2. **로그 탭**: 진행 상황 확인
3. **스캔 결과 탭**: 탐지된 개인정보 실시간 확인
   - 행 **더블클릭** → 파일 열기
   - 행 **우클릭** → 파일 열기 / 폴더 열기

### 4. 실행 (CLI)

```bat
build\PiiScanner.exe --path C:\Users\me\Documents --skip-images --output D:\Reports
```

---

## 출력 결과

```
pii_report_20260620_110000.xlsx   ← Excel (요약 · 파일목록 · 상세결과)
pii_report_20260620_110000.html   ← HTML 대화형 리포트
```

- **파일 경로 클릭** → 해당 문서 바로 열기 (HTML, Excel 모두)
- **탐지 유형 배지** → 파일별 검출된 개인정보 종류 색상 구분 표시
- 탐지 값은 자동 마스킹 (예: `850101-*******`)

---

## 테스트 결과

```
PiiScanner 단위 테스트
====================================================
결과: 28/28 통과 (100.0%)

[End-to-End 샘플 탐지]
✓ 주민등록번호 : 2건    ✓ 전화번호   : 2건
✓ 이메일       : 2건    ✓ IP주소     : 3건
✓ MAC주소      : 1건    ✓ 신용카드   : 2건
✓ 여권번호     : 1건    ✓ 운전면허   : 1건
→ 총 14건 / 8가지 유형 전수 탐지 성공
```

---

## 프로젝트 구조

```
docScanner/
├── build/
│   ├── PiiScanner.exe       ← CLI 실행파일
│   ├── PiiScannerUI.exe     ← GUI 실행파일
│   └── Everything64.dll
├── src/
│   ├── main.cpp             CLI 진입점
│   ├── main_ui.cpp          GUI 진입점 (Win32)
│   ├── everything_scanner   파일 목록화 (Everything SDK)
│   ├── text_extractor       텍스트 추출 (IFilter + WinOCR)
│   ├── pii_detector         PII 탐지 + 검증
│   ├── reporter             Excel + HTML 리포트 생성
│   └── xlsx_writer.h        OOXML 직접 구현 (의존성 제로)
├── build.bat                CLI 빌드 스크립트
├── build_ui.bat             GUI 빌드 스크립트
├── USAGE.md                 상세 사용 가이드
├── SECURE_CODING.md         보안 설계 문서
└── worklog.md               작업 일지
```

---

## 라이선스

MIT License — 자세한 내용은 [LICENSE](LICENSE) 참조.

> **주의**: 스캔 결과에는 개인정보가 포함될 수 있습니다.  
> 결과 파일을 안전하게 관리하고 불필요 시 즉시 삭제하세요.  
> 이 도구는 조직 내 개인정보 현황 파악 및 법적 준수를 위한 용도로만 사용하세요.
