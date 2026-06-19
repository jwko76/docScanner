@echo off
chcp 65001 > nul
echo.
echo PiiScanner - 빠른 실행 (사용자 폴더만, OCR 생략)
echo [보안] 레지스트리 미접근 / 네트워크 미사용 / 시스템 폴더 자동 제외
echo.
python pii_scanner.py --skip-images --output "%~dp0reports"
if not errorlevel 1 (
    echo.
    echo 완료! 결과: %~dp0reports
    explorer "%~dp0reports"
)
pause
