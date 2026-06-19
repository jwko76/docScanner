# PiiScanner — TODO / 이슈

## 🔴 필수 (실행 전 완료)

- [ ] **Everything64.dll 배치**
  - https://www.voidtools.com 에서 Everything-SDK.zip 다운로드
  - `dll/Everything64.dll` → `PiiScanner/` 폴더에 복사
  - C++ 빌드 버전이면 `PiiScanner/sdk/` 폴더에도 복사

- [ ] **의존성 설치**
  ```
  install.bat 실행  (또는 pip install -r requirements.txt)
  ```

- [ ] **Tesseract 설치** (이미지 OCR 필요 시)
  <!-- GitHub: https://github.com/YOUR_ID/docScanner -->
  - https://github.com/UB-Mannheim/tesseract/wiki
  - 설치 시 **Korean 언어팩** 반드시 선택

---

## 🟡 개선 우선순위 높음

- [ ] **HWP 추출 강화**
  - 현재: hwp5 라이브러리 → COM → olefile 순으로 시도
  - 개선: HWP 바이너리 포맷 직접 파싱 (hwp5 안정화)
  - 또는: LibreOffice CLI (`soffice --headless --convert-to txt`)로 변환

- [ ] **오탐 감소 — 계좌번호**
  - 현재 패턴이 전화번호/날짜/시리얼 번호와 겹침
  - 주요 은행 계좌번호 자리수 별도 패턴 적용 필요
  - 은행명/계좌 키워드 주변 컨텍스트 가중치 부여

- [ ] **오탐 감소 — 주소**
  - 현재 키워드 기반이라 텍스트 중간 출현 시 오탐 많음
  - 우편번호(5자리) 앞뒤 맥락 강화
  - JUSO API 또는 도로명주소 DB 연동 검토

- [ ] **PDF 이미지 페이지 처리**
  - pdfplumber로 텍스트 추출 실패 시 (스캔 PDF)
  - PyMuPDF(fitz) 로 페이지를 이미지로 변환 후 OCR

- [ ] **진행률 UI 개선**
  - 현재 tqdm 바 + 탐지 건수만 표시
  - 현재 처리 중인 파일명 표시 (tqdm `set_description`)
  - 예상 잔여 시간 정확도 개선

---

## 🟢 향후 기능 추가

- [ ] **화이트리스트 기능**
  - 특정 폴더/확장자 제외 (`--exclude` 옵션)
  - 특정 파일 내 패턴 무시 (예: 테스트 데이터 폴더)

- [ ] **증분 스캔 (변경 파일만)**
  - 이전 스캔 결과 DB 저장 (SQLite)
  - Everything의 `date_modified` 필터로 변경 파일만 재스캔

- [ ] **결과 비교 리포트**
  - 이전 스캔 vs 현재 스캔 차이 (신규/삭제/변경)
  - CI/CD 파이프라인 연동용 JSON 출력 (`--format json`)

- [ ] **네트워크 드라이브 지원**
  - Everything이 네트워크 경로를 인덱싱하는지 확인
  - 미인덱싱 경로는 `os.walk` fallback 스캐너로 처리

- [ ] **GUI 버전**
  - tkinter 또는 PyQt6로 간단한 UI
  - 실시간 탐지 현황 테이블 업데이트

- [ ] **C++ 빌드 자동화**
  - `setup_cpp.bat`: vcpkg 설치 + cmake 빌드 자동화
  - 릴리스 패키지: `PiiScanner_v1.0.zip` (exe + dll)

- [ ] **민감도 레벨 설정**
  - `--sensitivity high/medium/low`
  - high: 모든 패턴 + 낮은 신뢰도 포함
  - low: 체크섬 검증 통과한 패턴만

- [ ] **이메일 / 슬랙 알림**
  - 스캔 완료 후 요약 이메일 발송 (`--notify email@example.com`)

---

## 🐛 알려진 버그

- [ ] `_extract_ole_com` 에서 Excel COM 반환값이 tuple of tuple이라 `str()`이 지저분함
  - `ws.UsedRange.Value`를 직접 순회해서 join 처리 필요

- [ ] PPTX 노트(발표자 노트) 미추출
  - `shape.notes_slide` 접근 코드 추가 필요

- [ ] 매우 큰 XLSX (수십만 행)에서 openpyxl 메모리 초과 가능
  - `read_only=True` + 조기 종료 로직 강화 필요

- [ ] HTML 리포트에서 결과가 10만 건 이상이면 브라우저 렌더링 느림
  - 클라이언트 사이드 페이지네이션 또는 가상 스크롤 적용 필요

---

## ✅ 완료된 작업

- [x] Everything SDK ctypes 연동 (Python)
- [x] Everything SDK DLL 동적 로딩 (C++)
- [x] 문서 텍스트 추출 (docx/xlsx/pptx/pdf/hwp/rtf)
- [x] 이미지 OCR (Tesseract Python / WinOCR C++)
- [x] 한국어 인코딩 자동 감지 (UTF-8/UTF-16/EUC-KR)
- [x] 주민등록번호 체크섬 검증
- [x] 신용카드 Luhn 알고리즘 검증
- [x] IP 주소 유효성 필터
- [x] 멀티스레드 병렬 스캔
- [x] Excel 3시트 리포트 (xlsxwriter)
- [x] HTML 대화형 리포트 (탭 + 차트 + 마스킹)
- [x] CLI 인자 파싱 (`--path`, `--output`, `--skip-images` 등)
- [x] 포터블 Python 단일 파일 (`pii_scanner.py`)
- [x] install.bat / run.bat / run_quick.bat
- [x] 단위 테스트 28/28 통과 (여권번호 패턴 버그 수정 포함)
- [x] GitHub 형상관리 설정 (.gitignore / README.md / LICENSE / git_setup.bat)
- [x] 보안 설계 강화: 시스템 폴더 제외, 사용자 폴더 기본 스캔, 레지스트리/네트워크 미접근 코드 검증
