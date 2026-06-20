@echo off
chcp 65001 >nul
cd /d C:\Users\user\AppData\Roaming\Claude\local-agent-mode-sessions\144d51ba-068c-4233-8621-64696036a44a\13cdf1be-b906-4072-822a-c82534d6aa40\local_937e9a10-eae3-4c3a-84d4-9a759dc52d5f\outputs\PiiScanner

set MSVC_VER=14.51.36231
set VS_BASE=C:\Program Files\Microsoft Visual Studio\18\Insiders
set SDK_VER=10.0.26100.0
set SDK_BASE=C:\Program Files (x86)\Windows Kits\10

set CL_EXE="%VS_BASE%\VC\Tools\MSVC\%MSVC_VER%\bin\Hostx64\x64\cl.exe"

set INCLUDE="%VS_BASE%\VC\Tools\MSVC\%MSVC_VER%\include";"%SDK_BASE%\Include\%SDK_VER%\ucrt";"%SDK_BASE%\Include\%SDK_VER%\um";"%SDK_BASE%\Include\%SDK_VER%\shared";"%SDK_BASE%\Include\%SDK_VER%\cppwinrt";"%SDK_BASE%\Include\%SDK_VER%\winrt"

set LIB="%VS_BASE%\VC\Tools\MSVC\%MSVC_VER%\lib\x64";"%SDK_BASE%\Lib\%SDK_VER%\ucrt\x64";"%SDK_BASE%\Lib\%SDK_VER%\um\x64"

set PATH=%VS_BASE%\VC\Tools\MSVC\%MSVC_VER%\bin\Hostx64\x64;%PATH%

if exist build rmdir /s /q build
mkdir build

echo [컴파일 시작]
%CL_EXE% /EHsc /std:c++17 /O2 /W3 /MT /utf-8 /await /D_WIN32_WINNT=0x0A00 /DUNICODE /D_UNICODE /DNOMINMAX /DWIN32_LEAN_AND_MEAN /I"src" src\everything_scanner.cpp src\text_extractor.cpp src\pii_detector.cpp src\reporter.cpp src\main.cpp /Fe:"build\PiiScanner.exe" /Fo:"build\\" /link pathcch.lib shlwapi.lib ole32.lib oleaut32.lib query.lib windowsapp.lib shell32.lib

if errorlevel 1 (
    echo BUILD_FAILED
    exit /b 1
)
echo BUILD_OK
dir build\PiiScanner.exe
