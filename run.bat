@echo off
chcp 65001 > nul
echo.
echo ====================================================
echo   PiiScanner - 개인정보 스캐너
echo ====================================================
echo.
echo   [보안 설계]
echo   - Windows 시스템 폴더 자동 제외
echo   - 기본값: 내 사용자 폴더만 스캔
echo   - 레지스트리 미접근 / 스캔 중 네트워크 미사용
echo.
echo ====================================================
echo.
echo   스캔 범위를 선택하세요:
echo   1. 내 사용자 폴더 전체 (기본 권장 - Documents/Desktop/Downloads 등)
echo   2. 특정 폴더 지정
echo   3. 전체 드라이브 (주의: 시간이 오래 걸림)
echo.
set /p SCAN_CHOICE="  선택 (1/2/3, 기본 1): "
if "%SCAN_CHOICE%"=="" set SCAN_CHOICE=1

set SCAN_PATH=
set ALL_DRIVES_FLAG=

if "%SCAN_CHOICE%"=="2" (
    echo.
    echo   스캔할 폴더를 입력하세요:
    set /p SCAN_PATH="  경로: "
)
if "%SCAN_CHOICE%"=="3" (
    set ALL_DRIVES_FLAG=--all-drives
    echo.
    echo   [주의] 전체 드라이브 스캔을 선택했습니다.
    echo   Windows/Program Files/ProgramData 등 시스템 폴더는 자동 제외됩니다.
)

echo.
echo   추가로 제외할 폴더가 있으면 입력하세요 (없으면 Enter):
set /p EXCLUDE_PATH="  제외 경로: "
set EXCLUDE_FLAG=
if not "%EXCLUDE_PATH%"=="" set EXCLUDE_FLAG=--exclude "%EXCLUDE_PATH%"

echo.
echo   결과 저장 폴더 (기본: 현재 폴더\reports):
set /p OUT_PATH="  저장 위치: "
if "%OUT_PATH%"=="" set OUT_PATH=%~dp0reports

echo.
echo   이미지 OCR을 사용하시겠습니까? (y/n, 기본 y):
set /p USE_OCR="  선택: "
if /i "%USE_OCR%"=="n" (
    set OCR_FLAG=--skip-images
) else (
    set OCR_FLAG=
)

echo.
echo ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
if "%SCAN_CHOICE%"=="1" echo   스캔: 사용자 폴더 (%%USERPROFILE%%)
if "%SCAN_CHOICE%"=="2" echo   스캔: %SCAN_PATH%
if "%SCAN_CHOICE%"=="3" echo   스캔: 전체 드라이브 (시스템 폴더 제외)
echo   저장: %OUT_PATH%
echo ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
echo.

if "%SCAN_CHOICE%"=="1" (
    python pii_scanner.py --output "%OUT_PATH%" %OCR_FLAG% %EXCLUDE_FLAG%
) else if "%SCAN_CHOICE%"=="2" (
    if "%SCAN_PATH%"=="" (
        echo [오류] 경로를 입력하지 않았습니다.
        pause
        exit /b 1
    )
    python pii_scanner.py --path "%SCAN_PATH%" --output "%OUT_PATH%" %OCR_FLAG% %EXCLUDE_FLAG%
) else (
    python pii_scanner.py --all-drives --output "%OUT_PATH%" %OCR_FLAG% %EXCLUDE_FLAG%
)

echo.
if errorlevel 1 (
    echo [오류] 스캔 실패. install.bat 을 먼저 실행하세요.
) else (
    echo 완료! 결과: %OUT_PATH%
    explorer "%OUT_PATH%"
)
pause
