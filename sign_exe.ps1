# PiiScannerUI.exe 코드 서명 스크립트
# 자체 서명 인증서 생성 → Trusted Publishers/Root 등록 → exe 서명

$exePath = "D:\Work\AI\ClaudeCode\PiiScanner\build\PiiScannerUI.exe"

Write-Host "=== 1. 코드 서명 인증서 생성 ===" -ForegroundColor Cyan

# 기존 인증서가 있으면 재사용
$existing = Get-ChildItem Cert:\CurrentUser\My | Where-Object { $_.Subject -like "*docScanner*" }
if ($existing) {
    $cert = $existing | Select-Object -First 1
    Write-Host "  기존 인증서 재사용: $($cert.Thumbprint)"
} else {
    $cert = New-SelfSignedCertificate `
        -Subject "CN=docScanner PiiScanner, O=jwko76, C=KR" `
        -CertStoreLocation "Cert:\CurrentUser\My" `
        -Type CodeSigningCert `
        -KeyUsage DigitalSignature `
        -KeyAlgorithm RSA `
        -KeyLength 2048 `
        -HashAlgorithm SHA256 `
        -NotAfter (Get-Date).AddYears(10)
    Write-Host "  새 인증서 생성: $($cert.Thumbprint)"
}

Write-Host ""
Write-Host "=== 2. Trusted Root CA 등록 ===" -ForegroundColor Cyan
try {
    $rootStore = New-Object System.Security.Cryptography.X509Certificates.X509Store("Root", "CurrentUser")
    $rootStore.Open("ReadWrite")
    $rootStore.Add($cert)
    $rootStore.Close()
    Write-Host "  CurrentUser\Root 등록 완료"
} catch {
    Write-Host "  Root 등록 실패: $_" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "=== 3. Trusted Publishers 등록 ===" -ForegroundColor Cyan
try {
    $pubStore = New-Object System.Security.Cryptography.X509Certificates.X509Store("TrustedPublisher", "CurrentUser")
    $pubStore.Open("ReadWrite")
    $pubStore.Add($cert)
    $pubStore.Close()
    Write-Host "  CurrentUser\TrustedPublisher 등록 완료"
} catch {
    Write-Host "  TrustedPublisher 등록 실패: $_" -ForegroundColor Yellow
}

# LocalMachine에도 등록 시도 (관리자 권한 있을 경우)
try {
    $lmRoot = New-Object System.Security.Cryptography.X509Certificates.X509Store("Root", "LocalMachine")
    $lmRoot.Open("ReadWrite")
    $lmRoot.Add($cert)
    $lmRoot.Close()
    Write-Host "  LocalMachine\Root 등록 완료"

    $lmPub = New-Object System.Security.Cryptography.X509Certificates.X509Store("TrustedPublisher", "LocalMachine")
    $lmPub.Open("ReadWrite")
    $lmPub.Add($cert)
    $lmPub.Close()
    Write-Host "  LocalMachine\TrustedPublisher 등록 완료"
} catch {
    Write-Host "  LocalMachine 등록 건너뜀 (관리자 권한 불필요)" -ForegroundColor Gray
}

Write-Host ""
Write-Host "=== 4. signtool.exe 탐색 ===" -ForegroundColor Cyan

$signtool = $null
$sdkPaths = @(
    "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe",
    "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\signtool.exe",
    "C:\Program Files (x86)\Windows Kits\10\bin\x64\signtool.exe"
)
foreach ($p in $sdkPaths) {
    if (Test-Path $p) { $signtool = $p; break }
}
if (-not $signtool) {
    $signtool = (Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin" -Recurse -Filter "signtool.exe" -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match "x64" } | Select-Object -First 1).FullName
}
if (-not $signtool) {
    Write-Host "  signtool.exe 를 찾을 수 없음 — VS 경로 탐색 중..." -ForegroundColor Yellow
    $signtool = (Get-ChildItem "C:\Program Files\Microsoft Visual Studio" -Recurse -Filter "signtool.exe" -ErrorAction SilentlyContinue |
        Select-Object -First 1).FullName
}

if (-not $signtool) {
    Write-Host "  [오류] signtool.exe 없음. Windows SDK 설치 필요." -ForegroundColor Red
    exit 1
}
Write-Host "  signtool: $signtool"

Write-Host ""
Write-Host "=== 5. exe 서명 ===" -ForegroundColor Cyan

$thumb = $cert.Thumbprint
$result = & "$signtool" sign `
    /sha1 $thumb `
    /fd SHA256 `
    /td SHA256 `
    /tr "http://timestamp.digicert.com" `
    /d "PiiScanner - 개인정보 탐지 도구" `
    "$exePath" 2>&1

Write-Host $result

Write-Host ""
Write-Host "=== 6. 서명 검증 ===" -ForegroundColor Cyan
$verify = & "$signtool" verify /pa /v "$exePath" 2>&1
Write-Host $verify

Write-Host ""
Write-Host "완료!" -ForegroundColor Green
