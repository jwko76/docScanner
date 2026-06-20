Set-Location "D:\Work\AI\ClaudeCode\PiiScanner"

git add .

$msg = @"
feat: Win32 GUI + 인코딩 수정 + 파일 링크 기능 추가

- src/main_ui.cpp: Win32 GUI 실행파일 (PiiScannerUI.exe)
- build_ui.bat / build_ui.ps1: GUI 빌드 스크립트
- text_extractor.cpp: UTF-8 vs EUC-KR 인코딩 판별 개선 (시퀀스 검증)
- reporter.cpp: HTML/Excel 결과에 파일 클릭 링크 추가 (file:/// URL)
- xlsx_writer.h: HYPERLINK 수식 지원 (XLFMT_LINK, writeHyperlink)
"@

git commit -m $msg

$remote = git remote get-url origin
Write-Host "Remote: $remote"

git push origin main
if ($LASTEXITCODE -eq 0) {
    Write-Host "Push 완료!"
    # 토큰 제거 (원격 URL에서)
    $cleanRemote = $remote -replace 'https://[^@]+@', 'https://'
    git remote set-url origin $cleanRemote
} else {
    Write-Host "Push 실패 (exit code: $LASTEXITCODE)"
}
