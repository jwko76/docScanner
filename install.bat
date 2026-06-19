@echo off
chcp 65001 > nul
echo.
echo ====================================================
echo   PiiScanner 설치 스크립트
echo ====================================================
echo.

:: Python 확인
python --version > nul 2>&1
if errorlevel 1 (
    echo [오류] Python이 설치되지 않았습니다.
    echo   https://www.python.org/downloads/ 에서 설치 후 재실행하세요.
    pause
    exit /b 1
)

echo [1/3] Python 버전 확인...
python --version

echo.
echo [2/3] 패키지 설치 중...
pip install -r requirements.txt --quiet

if errorlevel 1 (
    echo [경고] 일부 패키지 설치 실패. hwp5는 선택사항이므로 무시합니다.
    pip install python-docx openpyxl python-pptx pdfplumber pypdf olefile pytesseract Pillow xlsxwriter tqdm chardet colorama --quiet
)

echo.
echo [3/3] Tesseract OCR 확인...
tesseract --version > nul 2>&1
if errorlevel 1 (
    echo [알림] Tesseract OCR가 설치되지 않았습니다.
    echo   이미지 OCR 기능을 사용하려면:
    echo   https://github.com/UB-Mannheim/tesseract/wiki 에서 설치
    echo   설치 시 'Korean' 언어팩 선택 필수
    echo.
    echo   OCR 없이도 문서 스캔은 정상 작동합니다.
) else (
    echo   Tesseract 설치 확인됨
    tesseract --version 2>&1 | findstr "tesseract"
)

echo.
echo ====================================================
echo   설치 완료! run.bat 으로 실행하세요.
echo ====================================================
echo.
pause
