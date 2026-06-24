@echo off
cd /d D:\Work\AI\ClaudeCode\PiiScanner
git add src/main_ui.cpp build_ui.ps1 sign_ui.bat sign_ui.ps1 find_sign.bat
git commit -m "feat(ui): keyword file search OR/AND/NOT, file load, auto-sign"
echo Done.
