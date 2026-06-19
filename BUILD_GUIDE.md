# PiiScanner 빌드 및 실행 가이드

## 아키텍처 개요

```
PiiScanner/
├── CMakeLists.txt          빌드 설정
├── vcpkg.json              패키지 의존성
├── BUILD_GUIDE.md          이 파일
├── sdk/
│   └── Everything64.dll   ← Everything SDK에서 복사
└── src/
    ├── Everything.h        Everything SDK 헤더
    ├── everything_scanner.h/.cpp   파일 목록 스캔 (Everything IPC)
    ├── text_extractor.h/.cpp       텍스트 추출 (IFilter + WinOCR)
    ├── pii_detector.h/.cpp         개인정보 탐지 (정규식 + 체크섬)
    ├── reporter.h/.cpp             Excel/HTML 리포트 생성
    └── main.cpp                    진입점 + 멀티스레드 오케스트레이션
```

---

## 사전 요구사항

| 항목 | 버전 | 용도 |
|------|------|------|
| Visual Studio | 2022 (Community 이상) | C++17 컴파일러, Windows SDK |
| CMake | 3.20+ | 빌드 시스템 |
| vcpkg | 최신 | libxlsxwriter 설치 |
| Everything | 1.4+ | 파일 빠른 검색 엔진 |
| Windows | 10 이상 | WinRT OCR API |
| Microsoft Office | 선택 | doc/xls/ppt IFilter 지원 |
| HWP Viewer | 선택 | hwp/hwpx IFilter 지원 |

---

## Step 1. 도구 설치

### 1-1. vcpkg 설치

```powershell
cd C:\
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install
```

환경변수 설정 (시스템 환경변수 → VCPKG_ROOT):
```
VCPKG_ROOT = C:\vcpkg
```

### 1-2. libxlsxwriter 설치

```powershell
cd C:\vcpkg
.\vcpkg install libxlsxwriter:x64-windows
```

---

## Step 2. Everything SDK 다운로드

1. https://www.voidtools.com/ko-kr/downloads/ 접속
2. **Everything SDK** 다운로드 (`Everything-SDK.zip`)
3. 압축 해제 후 `dll/` 폴더에서 `Everything64.dll` 복사
4. 프로젝트의 `sdk/` 폴더에 붙여넣기

```
PiiScanner/
└── sdk/
    └── Everything64.dll   ← 여기에 붙여넣기
```

---

## Step 3. 빌드

```powershell
cd PiiScanner

# 빌드 디렉토리 생성
cmake -B build -G "Visual Studio 17 2022" -A x64 `
      -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake"

# Release 빌드
cmake --build build --config Release
```

빌드 결과물: `build/Release/PiiScanner.exe`

---

## Step 4. 실행 전 준비

1. **Everything 실행**: 시작 메뉴에서 Everything 실행 (백그라운드 상태 유지)
2. **최초 색인 대기**: Everything DB 구축 완료까지 대기 (수 분 소요)
3. **Windows 한국어 OCR**: 설정 → 시간 및 언어 → 언어 → 한국어 추가 (이미지 OCR 필요 시)

---

## Step 5. 실행

### 전체 드라이브 스캔
```powershell
.\PiiScanner.exe --output "C:\Reports"
```

### 특정 폴더만 스캔
```powershell
.\PiiScanner.exe --path "C:\Users\홍길동\Documents" --output "C:\Reports"
```

### 이미지 OCR 건너뜀 (빠른 스캔)
```powershell
.\PiiScanner.exe --path "D:\업무자료" --skip-images --output "C:\Reports"
```

### 스레드 수 지정
```powershell
.\PiiScanner.exe --threads 8 --output "C:\Reports"
```

### 파일 크기 제한 변경 (200MB)
```powershell
.\PiiScanner.exe --max-size 200 --output "C:\Reports"
```

---

## 결과 파일

실행 후 `--output` 경로에 두 파일 생성:

| 파일 | 내용 |
|------|------|
| `pii_report_YYYYMMDD_HHMMSS.xlsx` | Excel 리포트 (요약/파일목록/상세결과 3시트) |
| `pii_report_YYYYMMDD_HHMMSS.html` | HTML 리포트 (브라우저에서 열기, 탭 형식) |

### Excel 시트 구성
- **요약**: 총 건수, 유형별 통계
- **파일 목록**: 파일별 탐지 건수, 추출 방법, 소요 시간
- **상세 결과**: 탐지 값, 마스킹 값, 줄 번호, 맥락 텍스트

---

## 탐지 유형 및 패턴

| 유형 | 패턴 예시 | 검증 |
|------|-----------|------|
| 주민등록번호 | `850101-1234567` | 체크섬 + 날짜 유효성 |
| 전화번호 | `010-1234-5678`, `02-123-4567` | 정규식 |
| 이메일 | `hong@example.com` | 정규식 |
| IP 주소 | `192.168.1.100` | 유효 범위 검증 |
| MAC 주소 | `AA:BB:CC:DD:EE:FF` | 정규식 |
| 신용카드 | `4532-1234-5678-9012` | Luhn 알고리즘 |
| 주소 | `서울시 강남구 역삼동 123` | 키워드 기반 |
| 계좌번호 | `110-123-456789` | 정규식 |
| 여권번호 | `M12345678` | 정규식 |
| 운전면허 | `11-20-123456-78` | 정규식 |

---

## 지원 파일 형식

### 문서
| 확장자 | 추출 방법 | 요구사항 |
|--------|-----------|----------|
| .docx / .pptx / .xlsx | IFilter → OOXML XML 직접 파싱 | MS Office (선택) |
| .doc / .xls / .ppt | Windows IFilter | MS Office 필요 |
| .pdf | Windows IFilter | Windows 10+ 내장 PDF 필터 |
| .hwp / .hwpx | Windows IFilter | HWP 뷰어 설치 필요 |
| .txt / .csv / .log | 직접 읽기 | 인코딩 자동 감지 (UTF-8, UTF-16, EUC-KR) |
| .xml / .json / .html | 직접 읽기 | — |

### 이미지 (OCR)
| 확장자 | 추출 방법 |
|--------|-----------|
| .jpg / .jpeg / .png / .tiff / .bmp | Windows OCR API (한국어/영어) |

---

## 성능 참고

| 시나리오 | 예상 속도 |
|----------|-----------|
| 텍스트 파일 50만 건 | 약 10~30분 (HDD 기준) |
| 이미지 OCR (SSD) | 파일당 약 0.5~2초 |
| Office 문서 (IFilter) | 파일당 약 0.1~1초 |

- Everything SDK 덕분에 파일 목록 조회는 수십만 건도 수초 이내 완료
- 멀티스레드 스캔으로 CPU 코어 수만큼 병렬 처리

---

## 문제 해결

**Q: "Everything이 실행 중이지 않습니다" 오류**
→ Everything을 실행하고 DB 로딩(트레이 아이콘 확인) 후 재시도

**Q: .hwp 파일 텍스트 추출 실패**
→ 한컴 HWP 뷰어(무료) 설치: https://www.hancom.com/

**Q: 이미지 OCR 결과 없음**
→ Windows 설정 → 언어 → 한국어 추가 후 언어 팩 다운로드

**Q: .pdf IFilter 없음**
→ Windows 10 1903 이상에는 기본 내장. 이전 버전은 Adobe Reader 설치

**Q: libxlsxwriter 링크 오류**
→ `vcpkg install libxlsxwriter:x64-windows` 재실행 후 빌드 클린

---

## 라이선스

- Everything SDK: 자체 라이선스 (voidtools.com)
- libxlsxwriter: FreeBSD License
- PiiScanner 소스코드: MIT License
