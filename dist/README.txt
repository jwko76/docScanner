======================================================
  PiiScanner - 개인정보/민감정보 탐지 도구 (포터블)
======================================================

[ 필요 환경 ]
  - Windows 10 / 11 (64비트)
  - 설치 불필요 — 이 폴더만 복사해서 사용

[ 포함 파일 ]
  PiiScannerUI.exe      GUI 버전 (권장)
  PiiScanner.exe        CLI 버전 (명령줄)
  Everything64.dll      파일 검색 엔진 (자동 사용)
  PiiScannerUI_실행.bat GUI 빠른 실행
  PiiScanner_CLI.bat    CLI 빠른 실행
  output/               스캔 결과 저장 폴더

[ GUI 사용법 ]
  1. PiiScannerUI_실행.bat 더블클릭
  2. "스캔 경로" → 찾아보기 → 검사할 폴더 선택
  3. "스캔 시작" 클릭
  4. 결과 그리드에서 파일명 더블클릭 → 해당 파일 열기 + 폴더 열기
  5. "HTML 리포트 열기" / "Excel 리포트 열기" 버튼으로 상세 결과 확인

[ CLI 사용법 ]
  PiiScanner_CLI.bat C:\Users\홍길동\Documents
  또는
  PiiScanner.exe --path C:\Users\홍길동\Documents --output output\

[ 탐지 항목 ]
  주민등록번호, 전화번호, 이메일, 계좌번호,
  신용카드번호, 주소, 여권번호, 운전면허번호
  + 이미지/스캔 파일(JPG, PNG, TIFF, BMP) OCR 탐지

[ 출력 결과 ]
  output\pii_report_YYYYMMDD_HHMMSS.html   (브라우저에서 열기)
  output\pii_report_YYYYMMDD_HHMMSS.xlsx   (Excel에서 열기)

[ 주의사항 ]
  - 결과 파일에 개인정보가 포함됩니다. 안전하게 관리하세요.
  - Everything SDK를 사용 시 voidtools Everything이 실행 중이면 더 빠릅니다.
  - 이미지 OCR은 Windows 기본 OCR 엔진(한국어 지원)을 사용합니다.

======================================================
