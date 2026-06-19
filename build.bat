@echo off
chcp 65001 >nul
echo.
echo ====================================================
echo   PiiScanner - C++ 빌드 (의존성 제로 / 정적 CRT)
echo ====================================================
echo.

:: ── MSVC 환경 탐색 ───────────────────────────────────────────
set VCVARS=

:: Visual Studio 18 Insiders (최신)
if exist "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat"
    goto :found
)
:: Visual Studio 18 BuildTools
if exist "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    goto :found
)
:: Visual Studio 2022
for %%E in (Community Professional Enterprise BuildTools) do (
    if exist "C:\Program Files\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat" (
        set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat"
        goto :found
    )
)
:: Visual Studio 2019
for %%E in (Community Professional Enterprise BuildTools) do (
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\%%E\VC\Auxiliary\Build\vcvars64.bat" (
        set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2019\%%E\VC\Auxiliary\Build\vcvars64.bat"
        goto :found
    )
)

echo [오류] Visual Studio 또는 Build Tools 를 찾을 수 없습니다.
echo        https://visualstudio.microsoft.com/downloads/ 에서 설치 후 재시도하세요.
echo        (C++ 데스크톱 개발 워크로드 선택)
pause
exit /b 1

:found
echo   [OK] MSVC 발견: %VCVARS%
echo.
call "%VCVARS%" >nul 2>&1

:: ── 출력 폴더 ─────────────────────────────────────────────────
if not exist build mkdir build

:: ── 컴파일 ───────────────────────────────────────────────────
echo   컴파일 중...
echo.

cl.exe ^
    /EHsc ^
    /std:c++20 ^
    /O2 ^
    /W3 ^
    /MT ^
    /utf-8 ^
    /D_WIN32_WINNT=0x0A00 ^
    /DUNICODE /D_UNICODE ^
    /DNOMINMAX ^
    /DWIN32_LEAN_AND_MEAN ^
    /I"src" ^
    src\everything_scanner.cpp ^
    src\text_extractor.cpp ^
    src\pii_detector.cpp ^
    src\reporter.cpp ^
    src\main.cpp ^
    /Fe:"build\PiiScanner.exe" ^
    /Fo:"build\\" ^
    /link ^
        pathcch.lib ^
        shlwapi.lib ^
        ole32.lib ^
        oleaut32.lib ^
        query.lib ^
        windowsapp.lib ^
        shell32.lib

if errorlevel 1 (
    echo.
    echo [오류] 컴파일 실패.
    echo        위 오류 메시지를 확인하세요.
    pause
    exit /b 1
)

:: ── Everything64.dll 복사 ────────────────────────────────────
if exist "sdk\Everything64.dll" (
    copy /Y "sdk\Everything64.dll" "build\Everything64.dll" >nul
    echo   [OK] Everything64.dll 복사 완료
) else (
    echo   [주의] sdk\Everything64.dll 없음 - 실행 전 복사 필요
    echo          https://www.voidtools.com/support/everything/sdk/ 에서 다운로드
)

echo.
echo ====================================================
echo   빌드 성공!
echo   실행 파일: build\PiiScanner.exe
echo ====================================================
echo.
echo   사용법:
echo     build\PiiScanner.exe                       (사용자 폴더 스캔)
echo     build\PiiScanner.exe --path C:\Users\me    (특정 경로)
echo     build\PiiScanner.exe --skip-images         (OCR 생략)
echo     build\PiiScanner.exe --output C:\Reports   (결과 저장 위치)
echo.
pause
