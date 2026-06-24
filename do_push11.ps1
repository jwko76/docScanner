cd "D:\Work\AI\ClaudeCode\PiiScanner"
$token = $env:GH_TOKEN
git -c credential.helper='' push "https://jwko76:$token@github.com/jwko76/docScanner.git" HEAD:main
git remote set-url origin https://github.com/jwko76/docScanner.git
Write-Host "Push done"
