$env:GIT_TERMINAL_PROMPT = "0"
cd "D:\Work\AI\ClaudeCode\PiiScanner"
git add src/everything_scanner.cpp
git commit -m "fix: secure coding review - sysGetEnvW two-call pattern, remove duplicate recycle.bin entry"
git push origin main
Write-Host "Done"
