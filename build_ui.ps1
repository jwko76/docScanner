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

Write-Host ""
Write-Host "====================================================" -ForegroundColor Green
Write-Host "  Build SUCCESS!" -ForegroundColor Green
Write-Host "  CLI: build\PiiScanner.exe" -ForegroundColor White
Write-Host "  GUI: build\PiiScannerUI.exe" -ForegroundColor White
Write-Host "====================================================" -ForegroundColor Green
Write-Host ""