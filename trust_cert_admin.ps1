# 관리자 권한으로 실행: 자체 서명 인증서를 LocalMachine 신뢰 저장소에 등록
$thumb = "4A9A5F218B2B4D90A78274B9C261FED3A9DC78E9"

# CurrentUser\My에서 인증서 가져오기
$cert = Get-Item "Cert:\CurrentUser\My\$thumb" -ErrorAction Stop
Write-Host "인증서: $($cert.Subject)"

# LocalMachine\Root (신뢰된 루트 CA)
$rootStore = New-Object System.Security.Cryptography.X509Certificates.X509Store("Root", "LocalMachine")
$rootStore.Open("ReadWrite")
$rootStore.Add($cert)
$rootStore.Close()
Write-Host "[OK] LocalMachine\Root 등록 완료"

# LocalMachine\TrustedPublisher
$pubStore = New-Object System.Security.Cryptography.X509Certificates.X509Store("TrustedPublisher", "LocalMachine")
$pubStore.Open("ReadWrite")
$pubStore.Add($cert)
$pubStore.Close()
Write-Host "[OK] LocalMachine\TrustedPublisher 등록 완료"

# 결과 검증
$sig = Get-AuthenticodeSignature "D:\Work\AI\ClaudeCode\PiiScanner\build\PiiScannerUI.exe"
Write-Host "서명 상태: $($sig.Status)"
Write-Host "완료"
