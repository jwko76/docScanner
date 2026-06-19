@echo off
chcp 65001 > nul
echo.
echo ====================================================
echo   docScanner - GitHub 초기 설정
echo ====================================================
echo.
echo GitHub에 먼저 'docScanner' 리포지토리를 생성하세요:
echo   https://github.com/new
echo   - Repository name: docScanner
echo   - 공개(Public) 또는 비공개(Private) 선택
echo   - README 자동 생성 체크 해제 (이미 있음)
echo.
set /p GH_USER="GitHub 사용자명 입력: "
echo.

:: git 초기화
git init
git config user.name "%GH_USER%"
echo.
echo GitHub 이메일 입력 (GitHub 계정 이메일):
set /p GH_EMAIL="이메일: "
git config user.email "%GH_EMAIL%"

:: 원격 저장소 연결
git remote add origin https://github.com/%GH_USER%/docScanner.git

:: 초기 커밋
git add .
git commit -m "feat: 초기 커밋 - PII 스캐너 v1.0

- Everything SDK 기반 고속 파일 스캔
- 문서 텍스트 추출 (docx/xlsx/pptx/pdf/hwp 등)
- 이미지 OCR (Tesseract)
- 개인정보 탐지 (주민번호/전화/이메일/IP/MAC/카드/주소 등)
- 단위 테스트 28/28 통과
- Excel + HTML 리포트 생성
- Python 포터블 단일 파일 버전"

:: main 브랜치로 push
git branch -M main
git push -u origin main

if errorlevel 1 (
    echo.
    echo [오류] push 실패. 아래를 확인하세요:
    echo   1. GitHub 리포지토리가 생성되어 있는지 확인
    echo   2. 인증: git credential manager 또는 Personal Access Token 필요
    echo      https://github.com/settings/tokens 에서 PAT 발급
    echo      push 시 비밀번호 입력란에 PAT 입력
) else (
    echo.
    echo ====================================================
    echo   GitHub push 완료!
    echo   https://github.com/%GH_USER%/docScanner
    echo ====================================================
)
echo.
pause
