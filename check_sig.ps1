$exe = "D:\Work\AI\ClaudeCode\PiiScanner\build\PiiScannerUI.exe"
$sig = Get-AuthenticodeSignature $exe
Write-Host "=== 서명 상태 ==="
Write-Host "Status      : $($sig.Status)"
Write-Host "StatusMessage: $($sig.StatusMessage)"
if ($sig.SignerCertificate) {
    Write-Host "Subject     : $($sig.SignerCertificate.Subject)"
    Write-Host "Issuer      : $($sig.SignerCertificate.Issuer)"
    Write-Host "Thumbprint  : $($sig.SignerCertificate.Thumbprint)"
    Write-Host "NotAfter    : $($sig.SignerCertificate.NotAfter)"
} else {
    Write-Host "서명 없음"
}
