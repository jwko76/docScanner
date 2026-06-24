$env:GIT_TERMINAL_PROMPT = "0"
cd "D:\Work\AI\ClaudeCode\PiiScanner"
git add src/everything_scanner.cpp src/everything_scanner.h src/main_ui.cpp
git commit -m "feat: system folder exclusion in full-drive scan mode"
Write-Host "Done"
