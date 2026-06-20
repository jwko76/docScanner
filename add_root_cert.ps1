$thumb = "4A9A5F218B2B4D90A78274B9C261FED3A9DC78E9"
$certFile = "D:\Work\AI\ClaudeCode\PiiScanner\build\piiscanner_sign.cer"

# 인증서를 파일로 내보내기
$cert = Get-Item "Cert:\CurrentUser\My\$thumb"
$certBytes = $cert.Export([System.Security.Cryptography.X509Certificates.X509ContentType]::Cert)
[System.IO.File]::WriteAllBytes($certFile, $certBytes)
Write-Host "인증서 내보내기: $certFile"

# certutil로 CurrentUser Root에 추가 (보안 경고 없이 시도)
$result = certutil -addstore -user Root $certFile 2>&1
Write-Host $result

# LocalMachine Root에도 추가 (관리자 필요)
$result2 = certutil -addstore Root $certFile 2>&1
Write-Host $result2

# 상태 재확인
$sig = Get-AuthenticodeSignature "D:\Work\AI\ClaudeCode\PiiScanner\build\PiiScannerUI.exe"
Write-Host "서명 상태: $($sig.Status)"
