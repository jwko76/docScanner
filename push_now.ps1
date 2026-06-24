cd D:\Work\AI\ClaudeCode\PiiScanner
$token = $env:GH_TOKEN
if ($token) {
    git remote set-url origin "https://$token@github.com/jwko76/docScanner.git"
    git push origin main
    git remote set-url origin "https://github.com/jwko76/docScanner.git"
} else {
    git push origin main
}
