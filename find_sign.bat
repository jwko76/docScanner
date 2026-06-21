@echo off
where signtool 2>nul
if %ERRORLEVEL%==0 goto :done
dir /s /b "C:\Program Files (x86)\Windows Kits\10\bin\signtool.exe" 2>nul
dir /s /b "C:\Program Files\Windows Kits\10\bin\signtool.exe" 2>nul
:done
