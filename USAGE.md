# PiiScanner 사용 가이드

## 개요

PiiScanner는 Everything SDK를 이용해 PC의 문서/이미지 파일을 고속 스캔하여  
주민등록번호, 신용카드번호, 전화번호 등 개인정보(PII)를 탐지하고 리포트를 생성하는 도구입니다.

**특징**
- 순수 C++ 구현 (외부 라이브러리 의존성 없음, 정적 CRT 링크)
- Excel + HTML 리포트 자동 생성
- Windows OCR API로 이미지 텍스트 추출 (Windows 10 이상)
- Windows IFilter로 Office/PDF 텍스트 추출

---

## 사전 요구사항

| 항목 | 버전 | 비고 |
|------|------|------|
| Windows | 10 이상 | WinRT OCR 사용 |
| [Everything](https://www.voidtools.com/) | 1.4 이상 | 백그라운드에서 실행 중이어야 함 |
| `Everything64.dll` | SDK | exe와 같은 폴더에 배치 |

### Everything64.dll 준비

1. [voidtools SDK 페이지](https://www.voidtools.com/support/everything/sdk/) 에서 `Everything-SDK.zip` 다운로드
2. `Everything64.dll` (또는 32비트 환경이면 `Everything32.dll`) 을 `build/` 폴더에 복사

```
build/
  PiiScanner.exe
  Everything64.dll   ← 여기에 배치
```

---

## 빌드

### 방법 1: build.bat (권장)

Visual Studio 또는 Build Tools가 설치된 환경에서:

```bat
build.bat
```

- MSVC를 자동 탐색 (VS18 Insiders → VS2022 → VS2019)
- `/std:c++20 /MT` — C++20 표준, 정적 CRT 링크
- 외부 패키지 없음 (vcpkg 불필요)
- 결과물: `build\PiiScanner.exe`

### 방법 2: CMake

```bat
mkdir cmake-build && cd cmake-build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

> **주의**: WinRT 헤더(`winrt/base.h`) 포함으로 인해 `text_extractor.cpp` 컴파일에 수 분 소요됩니다.

---

## 실행

### 기본 실행 (현재 사용자 폴더 스캔)

```bat
build\PiiScanner.exe
```

기본값:
- 스캔 경로: `%USERPROFILE%` (문서, 데스크톱, 다운로드 등)
- 리포트 저장: exe와 같은 폴더
- 이미지 OCR: 활성화

### 옵션

```
사용법: PiiScanner.exe [옵션]

  --path <경로>        스캔할 루트 경로 (기본: 전체 드라이브)
  --output <경로>      리포트 저장 폴더 (기본: exe 위치)
  --dll <경로>         Everything DLL 경로 직접 지정
  --skip-images        이미지 OCR 건너뜀 (빠른 스캔)
  --max-size <MB>      파일당 최대 크기 (기본: 100)
  --threads <N>        병렬 스캔 스레드 수 (기본: CPU 코어 수)
  --help, -h           도움말 출력
```

### 예시

```bat
:: 특정 폴더만 스캔
build\PiiScanner.exe --path C:\Users\jwko\Documents

:: OCR 없이 빠른 스캔, 결과를 D:\Reports에 저장
build\PiiScanner.exe --skip-images --output D:\Reports

:: 파일 크기 50MB 이하만, 스레드 8개
build\PiiScanner.exe --max-size 50 --threads 8

:: Everything DLL 경로 직접 지정
build\PiiScanner.exe --dll C:\Tools\Everything64.dll --path C:\Data
```

---

## 탐지 유형

| 유형 | 설명 | 검증 방식 |
|------|------|----------|
| 주민등록번호 | YYMMDD-NNNNNNN | 체크섬 + 날짜 유효성 |
| 신용카드번호 | 13~19자리 카드 번호 | Luhn 알고리즘 |
| 전화번호 | 국내 휴대폰/일반 | 정규식 |
| 이메일 | RFC 5322 기반 | 정규식 |
| IP 주소 | IPv4 (사설/루프백 제외) | 범위 필터 |
| MAC 주소 | XX:XX:XX:XX:XX:XX | 정규식 |
| 계좌번호 | 주요 은행 자리수 패턴 | 정규식 |
| 여권번호 | M/S/E + 8자리 | 정규식 |
| 운전면허번호 | 지역코드-연도-일련번호 | 정규식 |
| 한국 주소 | 광역시/도 + 구/동/로 | 키워드 기반 |

---

## 출력 파일

스캔 완료 후 출력 폴더에 두 가지 리포트가 생성됩니다.

```
pii_report_20260619_110000.xlsx   ← Excel 리포트 (3개 시트)
pii_report_20260619_110000.html   ← HTML 대화형 리포트
```

### Excel 리포트 구조

| 시트 | 내용 |
|------|------|
| 요약 | 스캔 경로, 일시, 파일 수, 탐지 건수, 유형별 통계 |
| 파일 목록 | 파일별 경로, 확장자, 탐지 건수, 추출 방법, 오류 |
| 상세 결과 | 탐지 건별 유형, 마스킹된 원문, 줄번호, 신뢰도, 맥락 |

### HTML 리포트 구조

- 요약 카드 (스캔 파일 수, 개인정보 파일 수, 탐지 건수, 소요 시간)
- 탭 전환: 유형별 통계 차트 / 파일 목록 테이블 / 상세 결과 테이블
- 탐지 텍스트는 자동 마스킹 (예: `850101-*******`)

---

## 지원 파일 형식

| 범주 | 확장자 |
|------|--------|
| 문서 | `.hwp` `.hwpx` `.doc` `.docx` `.xls` `.xlsx` `.ppt` `.pptx` `.pdf` `.rtf` `.odt` |
| 텍스트 | `.txt` `.log` `.csv` `.tsv` `.xml` `.json` `.html` `.ini` `.cfg` `.conf` |
| 이미지 | `.jpg` `.jpeg` `.png` `.tif` `.tiff` `.bmp` `.gif` `.webp` |

텍스트 추출 방식:
- **일반 텍스트**: 직접 읽기 (UTF-8 / UTF-16 / EUC-KR 자동 감지)
- **Office/PDF/HWP**: Windows IFilter (설치된 Office에 따라 지원 범위 다름)
- **OOXML** (docx/xlsx/pptx): IFilter 실패 시 ZIP+XML 직접 파싱 fallback
- **이미지**: Windows OCR API (ko-KR 우선, 없으면 en-US)

---

## 보안 참고사항

- `--dll` 경로는 `.dll` 확장자 파일만 허용됩니다 (임의 DLL 로딩 방지).
- `--max-size` 값은 1~10,240 MB 범위만 허용됩니다.
- `--threads` 값은 1~256 범위만 허용됩니다.
- 스캔 결과 리포트에는 개인정보가 포함되므로 안전한 위치에 저장하세요.
- 자세한 보안 설계는 `SECURE_CODING.md`를 참조하세요.

---

## 알려진 제한사항

- Everything이 실행 중이지 않으면 파일 탐색 실패 (오류 메시지 출력 후 종료)
- HWP/HWPX는 IFilter 설치 여부에 따라 추출 품질 차이 있음
- 이미지 OCR은 인쇄 품질이 낮거나 회전된 이미지에서 정확도 저하
- 암호화된 Office 문서는 추출 불가
- 100MB 이상 파일은 기본적으로 건너뜀 (`--max-size`로 조정 가능)
