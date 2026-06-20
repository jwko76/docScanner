$exe = "D:\Work\AI\ClaudeCode\PiiScanner\build\PiiScannerUI.exe"

# signtool 탐색
$signtool = $null
Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin" -Recurse -Filter "signtool.exe" -ErrorAction SilentlyContinue | ForEach-Object {
    if ($_.FullName -match "x64") { $signtool = $_.FullName }
}
Write-Host "signtool: $signtool"

# 서명 검증
if ($signtool) {
    & $signtool verify /pa /v $exe
} else {
    Write-Host "[오류] signtool 없음"
}

# 서명 정보 (PowerShell 방식)
$sig = Get-AuthenticodeSignature $exe
Write-Host "서명 상태: $($sig.Status)"
Write-Host "서명자: $($sig.SignerCertificate.Subject)"
Write-Host "발급자: $($sig.SignerCertificate.Issuer)"
