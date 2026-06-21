$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path

# 기존 서명용 인증서 찾기 (CurrentUser\Root 또는 My)
$cert = Get-ChildItem Cert:\CurrentUser\Root | Where-Object { $_.Subject -match 'PiiScanner' } | Select-Object -First 1
if (-not $cert) {
    $cert = Get-ChildItem Cert:\CurrentUser\My | Where-Object { $_.Subject -match 'PiiScanner' -and $_.HasPrivateKey } | Select-Object -First 1
}

if (-not $cert) {
    Write-Host "[INFO] Creating new self-signed certificate..."
    $cert = New-SelfSignedCertificate `
        -DnsName "docScanner PiiScanner" `
        -CertStoreLocation Cert:\CurrentUser\My `
        -Type CodeSigningCert `
        -KeySpec Signature `
        -KeyUsage DigitalSignature `
        -KeyAlgorithm RSA `
        -KeyLength 2048
    
    # CurrentUser\Root 에 추가 (Smart App Control 통과)
    $certBytes = $cert.Export([System.Security.Cryptography.X509Certificates.X509ContentType]::Cert)
    $tmpCer = "$env:TEMP\piiscanner_code.cer"
    [System.IO.File]::WriteAllBytes($tmpCer, $certBytes)
    certutil -addstore -user Root $tmpCer | Out-Null
    Remove-Item $tmpCer -ErrorAction SilentlyContinue
    Write-Host "[OK] Certificate created: $($cert.Thumbprint)"
} else {
    Write-Host "[OK] Found certificate: $($cert.Thumbprint)"
}

# 서명 (타임스탬프 서버 오류 시 /t 없이 재시도)
$exe = "$root\build\PiiScannerUI.exe"
Write-Host "[INFO] Signing $exe ..."
try {
    & signtool sign /sha1 $cert.Thumbprint /fd SHA256 /t http://timestamp.sectigo.com "$exe" 2>&1
} catch {
    Write-Host "[WARN] Timestamp failed, signing without timestamp..."
    & signtool sign /sha1 $cert.Thumbprint /fd SHA256 "$exe" 2>&1
}

# 검증
$sig = Get-AuthenticodeSignature "$exe"
Write-Host "Signature status: $($sig.Status)"
