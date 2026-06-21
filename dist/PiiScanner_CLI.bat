@echo off
chcp 65001 > nul
cd /d "%~dp0"
if "%~1"=="" (
    echo 사용법: PiiScanner_CLI.bat [스캔할_폴더_경로]
    echo 예시:   PiiScanner_CLI.bat C:\Users\user\Documents
    echo.
    pause
    exit /b
)
PiiScanner.exe --path "%~1" --output "%~dp0output"
echo.
echo 완료. output 폴더에 결과가 저장되었습니다.
pause
