@echo off
chcp 65001 >nul
echo.
echo ====================================================
echo   PiiScanner - C++ ë¹Œë“œ (?˜ì¡´???œë¡œ / ?•ì  CRT)
echo ====================================================
echo.

:: ?€?€ MSVC ?˜ê²½ ?ìƒ‰ ?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€
set VCVARS=

:: Visual Studio 18 Insiders (ìµœì‹ )
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

echo [?¤ë¥˜] Visual Studio ?ëŠ” Build Tools ë¥?ì°¾ì„ ???†ìŠµ?ˆë‹¤.
echo        https://visualstudio.microsoft.com/downloads/ ?ì„œ ?¤ì¹˜ ???¬ì‹œ?„í•˜?¸ìš”.
echo        (C++ ?°ìŠ¤?¬í†± ê°œë°œ ?Œí¬ë¡œë“œ ? íƒ)
pause
exit /b 1

:found
echo   [OK] MSVC ë°œê²¬: %VCVARS%
echo.
call "%VCVARS%" >nul 2>&1

:: ?€?€ ì¶œë ¥ ?´ë” ?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€
if not exist build mkdir build

:: ?€?€ ì»´íŒŒ???€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€
echo   ì»´íŒŒ??ì¤?..
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
    echo [?¤ë¥˜] ì»´íŒŒ???¤íŒ¨.
    echo        ???¤ë¥˜ ë©”ì‹œì§€ë¥??•ì¸?˜ì„¸??
    pause
    exit /b 1
)

:: ?€?€ Everything64.dll ë³µì‚¬ ?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€?€
if exist "sdk\Everything64.dll" (
    copy /Y "sdk\Everything64.dll" "build\Everything64.dll" >nul
    echo   [OK] Everything64.dll ë³µì‚¬ ?„ë£Œ
) else (
    echo   [ì£¼ì˜] sdk\Everything64.dll ?†ìŒ - ?¤í–‰ ??ë³µì‚¬ ?„ìš”
    echo          https://www.voidtools.com/support/everything/sdk/ ?ì„œ ?¤ìš´ë¡œë“œ
)

echo.
echo ====================================================
echo   ë¹Œë“œ ?±ê³µ!
echo   ?¤í–‰ ?Œì¼: build\PiiScanner.exe
echo ====================================================
echo.
echo   ?¬ìš©ë²?
echo     build\PiiScanner.exe                       (?¬ìš©???´ë” ?¤ìº”)
echo     build\PiiScanner.exe --path C:\Users\me    (?¹ì • ê²½ë¡œ)
echo     build\PiiScanner.exe --skip-images         (OCR ?ëµ)
echo     build\PiiScanner.exe --output C:\Reports   (ê²°ê³¼ ?€???„ì¹˜)
echo.
pause
