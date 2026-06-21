@echo off
setlocal
set SIGNTOOL="C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe"
set THUMB=4A9A5F218B2B4D90A78274B9C261FED3A9DC78E9
set EXE=D:\Work\AI\ClaudeCode\PiiScanner\build\PiiScannerUI.exe

echo Signing %EXE% ...
%SIGNTOOL% sign /sha1 %THUMB% /fd SHA256 /t http://timestamp.sectigo.com %EXE% 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo Timestamp failed, signing without timestamp...
    %SIGNTOOL% sign /sha1 %THUMB% /fd SHA256 %EXE%
)
%SIGNTOOL% verify /pa %EXE%
echo Done.
