cd D:\Work\AI\ClaudeCode\PiiScanner
$token = $env:GH_TOKEN
if ($token) {
    git remote set-url origin "https://$($token)@github.com/jwko76/docScanner.git"
    git push origin main
    git remote set-url origin "https://github.com/jwko76/docScanner.git"
    Write-Host "Push complete (token used)"
} else {
    # credential manager 사용
    $env:GIT_TERMINAL_PROMPT = "0"
    git push origin main
    Write-Host "Push complete (credential manager)"
}
