$ErrorActionPreference = 'Stop'
Write-Host ""
Write-Host "====================================================" -ForegroundColor Cyan
Write-Host "  PiiScanner UI - Win32 GUI Build" -ForegroundColor Cyan
Write-Host "====================================================" -ForegroundColor Cyan
Write-Host ""

$vcvars = $null
$candidates = @(
    'C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat',
    'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat'
)
foreach ($e in @('Community','Professional','Enterprise','BuildTools')) {
    $candidates += "C:\Program Files\Microsoft Visual Studio\2022\$e\VC\Auxiliary\Build\vcvars64.bat"
    $candidates += "C:\Program Files (x86)\Microsoft Visual Studio\2019\$e\VC\Auxiliary\Build\vcvars64.bat"
}
foreach ($c in $candidates) { if (Test-Path $c) { $vcvars = $c; break } }
if (-not $vcvars) { Write-Error "Visual Studio not found"; exit 1 }
Write-Host "  [OK] MSVC: $vcvars" -ForegroundColor Green

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not (Test-Path "$root\build")) { New-Item -ItemType Directory "$root\build" | Out-Null }

$srcs = "src\everything_scanner.cpp src\text_extractor.cpp src\pii_detector.cpp src\reporter.cpp src\main_ui.cpp"
$flags = "/EHsc /std:c++20 /O2 /W3 /MT /utf-8 /D_WIN32_WINNT=0x0A00 /DUNICODE /D_UNICODE /DNOMINMAX /DWIN32_LEAN_AND_MEAN /Isrc"
$out   = "/Fe:build\PiiScannerUI.exe /Fo:build\\"
$libs  = "pathcch.lib shlwapi.lib shell32.lib ole32.lib oleaut32.lib query.lib windowsapp.lib user32.lib gdi32.lib comctl32.lib comdlg32.lib"
$link  = "/link /SUBSYSTEM:WINDOWS $libs"

$cmd = "`"$vcvars`" && cd /d `"$root`" && cl.exe $flags $srcs $out $link"
Write-Host "  Compiling..." -ForegroundColor Yellow
cmd /c $cmd
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed (exit $LASTEXITCODE)"; exit 1 }

if (Test-Path "$root\sdk\Everything64.dll") {
    Copy-Item "$root\sdk\Everything64.dll" "$root\build\Everything64.dll" -Force
    Write-Host "  [OK] Everything64.dll copied" -ForegroundColor Green
}

# 코드 서명 (자동 실행, Smart App Control 통과)
$signtool = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe"
if (Test-Path $signtool) {
    # 기존 인증서 탐색 (PiiScanner 주제명)
    $cert = Get-ChildItem Cert:\CurrentUser\Root |
        Where-Object { $_.Subject -match 'PiiScanner' } |
        Select-Object -First 1
    if (-not $cert) {
        $cert = Get-ChildItem Cert:\CurrentUser\My |
            Where-Object { $_.Subject -match 'PiiScanner' -and $_.HasPrivateKey } |
            Select-Object -First 1
    }
    if (-not $cert) {
        # 인증서 신규 생성
        $cert = New-SelfSignedCertificate `
            -DnsName "docScanner PiiScanner" `
            -CertStoreLocation Cert:\CurrentUser\My `
            -Type CodeSigningCert -KeySpec Signature `
            -KeyUsage DigitalSignature -KeyAlgorithm RSA -KeyLength 2048
        $tmpCer = "$env:TEMP\piiscanner_code.cer"
        [IO.File]::WriteAllBytes($tmpCer, $cert.Export('Cert'))
        certutil -addstore -user Root $tmpCer | Out-Null
        Remove-Item $tmpCer -ErrorAction SilentlyContinue
        Write-Host "  [OK] New signing certificate created" -ForegroundColor Green
    }
    $guiExe = "$root\build\PiiScannerUI.exe"
    & $signtool sign /sha1 $cert.Thumbprint /fd SHA256 /t http://timestamp.sectigo.com "$guiExe" 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        & $signtool sign /sha1 $cert.Thumbprint /fd SHA256 "$guiExe" 2>&1 | Out-Null
    }
    $status = (Get-AuthenticodeSignature "$guiExe").Status
    Write-Host "  [OK] Signed: $status" -ForegroundColor Green
}

Write-Host ""
Write-Host "====================================================" -ForegroundColor Green
Write-Host "  Build SUCCESS!" -ForegroundColor Green
Write-Host "  CLI: build\PiiScanner.exe" -ForegroundColor White
Write-Host "  GUI: build\PiiScannerUI.exe" -ForegroundColor White
Write-Host "====================================================" -ForegroundColor Green
Write-Host ""