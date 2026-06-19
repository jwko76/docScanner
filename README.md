# docScanner

> 개인정보 보호법(PIPA) · GDPR 준수를 위한 문서 내 개인정보/민감정보 탐지 도구

[![Python](https://img.shields.io/badge/Python-3.8%2B-blue)](https://python.org)
[![License: MIT](https://img.shields.io/badge/License-MIT-green)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%2B-lightgrey)](https://microsoft.com)

---

## 개요

로컬 컴퓨터에 저장된 파일을 빠르게 스캔하여 개인정보 및 민감정보가 포함된 파일을 탐지합니다.  
[Everything](https://www.voidtools.com/) SDK를 이용해 수십만 파일도 수초 내에 목록화하고, 다양한 문서·이미지 형식에서 텍스트를 추출하여 정규식 + 체크섬 알고리즘으로 PII를 탐지합니다.

---

## 주요 기능

| 기능 | 설명 |
|------|------|
| **고속 파일 스캔** | Everything SDK IPC로 50만 파일도 수초 내 목록화 |
| **다양한 문서 지원** | hwp/hwpx · doc/docx · xls/xlsx · ppt/pptx · pdf · txt · csv 등 |
| **이미지 OCR** | Tesseract OCR로 이미지(jpg/png/tiff) 내 텍스트 추출 |
| **PII 탐지** | 주민등록번호·전화번호·이메일·IP/MAC·신용카드·주소 등 10가지 |
| **검증 알고리즘** | 주민번호 체크섬, 신용카드 Luhn, IP 유효성으로 오탐 최소화 |
| **멀티스레드** | CPU 코어 수에 맞춰 병렬 스캔 |
| **이중 리포트** | Excel 3시트 + 대화형 HTML (마스킹·탭·차트 포함) |
| **포터블** | Python 단일 파일, 의존성 자동 설치 |

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
| 주소 | `서울시 강남구 테헤란로 123` | 광역시/도 키워드 |
| 계좌번호 | `110-123-456789` | 은행 자릿수 패턴 |
| 여권번호 | `M12345678` | 패턴 (한국 M+8자리) |
| 운전면허 | `11-20-123456-78` | 지역코드 패턴 |

---

## 요구사항

- **Python** 3.8 이상
- **Windows** 10 이상
- **[Everything](https://www.voidtools.com/)** 실행 중 (백그라운드)
- **Everything SDK** — `Everything64.dll` ([다운로드](https://www.voidtools.com/ko-kr/downloads/))
- **Tesseract OCR** (선택, 이미지 스캔 시) — [설치 가이드](https://github.com/UB-Mannheim/tesseract/wiki) + Korean 언어팩

### Python 패키지
```
python-docx, openpyxl, python-pptx, pdfplumber,
pytesseract, Pillow, xlsxwriter, tqdm, chardet, colorama
```

---

## 빠른 시작

### 1. 설치
```bash
git clone https://github.com/YOUR_ID/docScanner.git
cd docScanner
install.bat          # 자동 설치 (또는: pip install -r requirements.txt)
```

### 2. Everything64.dll 배치
```
https://www.voidtools.com/ 에서 Everything-SDK.zip 다운로드
→ dll/Everything64.dll 파일을 프로젝트 루트에 복사
```

### 3. 실행
```bash
# 대화형 실행
run.bat

# CLI 실행
python pii_scanner.py --path "C:\Users\홍길동\Documents" --output "C:\Reports"

# 빠른 실행 (OCR 생략)
python pii_scanner.py --path "D:\업무자료" --skip-images --output "C:\Reports"

# 전체 드라이브 스캔
python pii_scanner.py --output "C:\Reports"
```

### CLI 옵션
| 옵션 | 기본값 | 설명 |
|------|--------|------|
| `--path <경로>` | 전체 드라이브 | 스캔할 폴더 |
| `--output <경로>` | `./reports` | 리포트 저장 폴더 |
| `--skip-images` | False | 이미지 OCR 건너뜀 |
| `--max-size <MB>` | 100 | 최대 파일 크기 |
| `--threads <N>` | 자동 | 병렬 스레드 수 |

---

## 출력 결과

실행 후 `--output` 경로에 두 파일이 생성됩니다:

```
reports/
├── pii_report_20260619_143022.xlsx   ← Excel (3시트)
└── pii_report_20260619_143022.html   ← HTML 대화형 리포트
```

### Excel 구성
- **요약**: 탐지 건수, 유형별 통계, 소요 시간
- **파일 목록**: 파일별 탐지 건수, 추출 방법, 오류
- **상세 결과**: 탐지 값, 마스킹 값, 줄 번호, 맥락 텍스트

### HTML 구성
- 탭 3개 (통계/파일목록/상세)
- 유형별 막대 차트
- 마스킹 처리된 PII 값 (원본은 Excel에만)

---

## 지원 파일 형식

| 형식 | 추출 방법 | 요구사항 |
|------|-----------|----------|
| `.docx` | python-docx | — |
| `.xlsx` | openpyxl | — |
| `.pptx` | python-pptx | — |
| `.pdf` | pdfplumber | — |
| `.hwp` | hwp5 → COM | 한컴 뷰어/오피스 (선택) |
| `.hwpx` | ZIP+XML 직접 파싱 | — |
| `.doc/.xls/.ppt` | olefile → COM | MS Office (선택) |
| `.txt/.csv/.log` | 직접 읽기 | 인코딩 자동 감지 |
| `.jpg/.png/.tiff` | Tesseract OCR | Tesseract + 한국어팩 |

---

## C++ 고성능 버전

컴파일이 가능한 환경이라면 `src/` 폴더의 C++ 버전이 더 빠릅니다.

```bash
# vcpkg + CMake 빌드
vcpkg install libxlsxwriter:x64-windows
cmake -B build -G "Visual Studio 17 2022" -A x64 \
      -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
```

자세한 내용은 [BUILD_GUIDE.md](BUILD_GUIDE.md) 참조.

---

## 테스트 결과

```
PiiScanner 단위 테스트 (버그 수정 후)
====================================================
결과: 28/28 통과 (100.0%)

[End-to-End 샘플 탐지]
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

## 프로젝트 구조

```
docScanner/
├── pii_scanner.py          ★ 메인 (Python 포터블 단일 파일)
├── requirements.txt
├── install.bat / run.bat / run_quick.bat
├── src/                    C++ 고성능 버전
│   ├── everything_scanner.h/.cpp
│   ├── text_extractor.h/.cpp
│   ├── pii_detector.h/.cpp
│   ├── reporter.h/.cpp
│   └── main.cpp
├── CMakeLists.txt
├── claude.md               AI 작업 컨텍스트
├── todo.md                 이슈/개선 목록
└── worklog.md              작업 일지
```

---

## 라이선스

MIT License — 자세한 내용은 [LICENSE](LICENSE) 참조.

> **주의**: 스캔 결과에는 개인정보가 포함될 수 있습니다.  
> 결과 파일을 안전하게 관리하고 불필요 시 즉시 삭제하세요.  
> 이 도구는 조직 내 개인정보 현황 파악 및 법적 준수를 위한 용도로만 사용하세요.

---

## 기여

Issue 및 PR 환영합니다. 기여 전 `todo.md`의 알려진 이슈를 먼저 확인해 주세요.
