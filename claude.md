# PiiScanner — Claude 작업 컨텍스트

> 이 파일은 Claude가 프로젝트를 재개할 때 바로 문맥을 파악하기 위한 참조 문서입니다.

---

## 프로젝트 개요

개인 컴퓨터에 저장된 파일에서 **개인정보 및 민감정보를 탐지**하는 스캐너.

- **파일 목록 조회**: Everything SDK (voidtools) IPC → 수십만 파일을 수초 내 스캔
- **텍스트 추출**: 문서(hwp/doc/xlsx/pdf 등) + 이미지(OCR)
- **개인정보 탐지**: 정규식 + 체크섬 검증
- **결과 출력**: Excel 3시트 + 대화형 HTML 리포트

---

## 파일 구조

```
PiiScanner/
├── pii_scanner.py          ★ 포터블 메인 (Python, 즉시 실행)
├── requirements.txt        pip 의존성
├── install.bat             자동 설치 스크립트
├── run.bat                 대화형 실행기
├── run_quick.bat           빠른 실행 (OCR 생략)
│
├── src/                    C++ 버전 (고성능, 컴파일 필요)
│   ├── Everything.h        Everything SDK 함수 포인터 선언
│   ├── everything_scanner.h/.cpp    파일 스캔 모듈
│   ├── text_extractor.h/.cpp        텍스트 추출 (IFilter + WinOCR)
│   ├── pii_detector.h/.cpp          PII 탐지 (regex + 검증)
│   ├── reporter.h/.cpp              Excel/HTML 리포트
│   └── main.cpp                     CLI 진입점
│
├── CMakeLists.txt          C++ 빌드 설정
├── vcpkg.json              C++ 의존성 (libxlsxwriter만)
├── sdk/                    Everything64.dll 위치 (직접 복사 필요)
│
├── claude.md               ← 이 파일
├── todo.md                 할 일 / 이슈
└── worklog.md              작업 기록
```

---

## 핵심 설계 결정

### 파일 스캔 — Everything SDK
- `Everything64.dll`을 `ctypes.WinDLL()`로 동적 로딩 (DLL 미설치 환경 안전)
- IPC 블로킹 쿼리 (`Everything_QueryW(True)`) → 50만 파일도 수초 내 완료
- 검색 쿼리: `ext:docx | ext:pdf | ext:jpg | ...` 형태로 확장자 필터링

### 텍스트 추출 — 라이브러리 우선순위
| 포맷 | Python | C++ |
|------|--------|-----|
| .docx | python-docx | IFilter → OOXML XML |
| .xlsx | openpyxl | IFilter → OOXML XML |
| .pptx | python-pptx | IFilter → OOXML XML |
| .pdf | pdfplumber | Windows PDF IFilter |
| .hwp | hwp5 → COM → olefile | IFilter (HWP뷰어) |
| .doc/.xls | olefile → COM | IFilter |
| 이미지 | Tesseract (pytesseract) | Windows OCR API (WinRT) |
| 텍스트 | chardet 인코딩 감지 | MultiByteToWideChar |

### PII 탐지 패턴
| 유형 | 검증 방식 | 오탐 방지 |
|------|-----------|-----------|
| 주민등록번호 | 체크섬 (가중치 합) + 날짜 범위 | ✓ 강함 |
| 신용카드 | Luhn 알고리즘 | ✓ 강함 |
| IP 주소 | 0.0.0.0, 255.x.x.x 제외 | △ 보통 |
| 전화번호 | 국내 국번 패턴 | △ 보통 |
| 이메일 | RFC 단순 패턴 | △ 보통 |
| 주소 | 광역시/도 키워드 + 숫자 | △ 약함 (오탐 多) |
| 계좌번호 | 정규식만 | ✗ 약함 (오탐 가능) |

### 리포트 구조
- **Excel**: 요약 / 파일 목록 / 상세 결과 (3시트, xlsxwriter)
- **HTML**: 탭 3개 + 유형별 막대 차트 + 마스킹 표시 (순수 JS, 외부 의존 없음)

### 멀티스레드
- Python: `concurrent.futures.ThreadPoolExecutor` (GIL 제약으로 I/O 병렬만 효과적)
- C++: `std::thread` 워커 풀 (CPU 바운드도 병렬)

---

## 실행 방법 (Python 포터블)

```bash
# 1. 설치
install.bat   # 또는: pip install -r requirements.txt

# 2. Everything64.dll 복사 (SDK zip에서)
#    → pii_scanner.py와 같은 폴더에 배치

# 3. Everything 실행 (백그라운드)

# 4. 스캔
python pii_scanner.py --path "D:\업무자료" --output "C:\Reports"
# 또는
run.bat       # 대화형 실행
run_quick.bat # OCR 없이 빠른 실행
```

---

## 알려진 제약 / 주의사항

- Everything이 백그라운드에서 **반드시 실행 중**이어야 함
- HWP 파일은 **한컴 뷰어** 또는 **한컴 오피스** 설치 시 정상 추출
- 구버전 .doc/.xls/.ppt는 **Office 설치** 시 COM 자동 사용
- 이미지 OCR은 **Tesseract + Korean 언어팩** 필요
- 주소 패턴은 오탐 가능성이 높으므로 결과 검토 필요
- EUC-KR 파일은 `chardet`으로 자동 감지하나 간혹 오인식 가능

---

## 향후 개선 방향 (→ todo.md 참조)
