Set-Location "D:\Work\AI\ClaudeCode\PiiScanner"

# vcpkg 추적 제거
git rm --cached vcpkg -r --quiet 2>$null

git add .

$msg = @"
docs: 문서 업데이트 + vcpkg gitignore 추가

- USAGE.md: GUI 버전 섹션, 파일 클릭 링크, 인코딩 수정 내용 추가 / vcpkg 주의사항 기재
- worklog.md: 세션 5(GUI+인코딩+링크), 세션 6(문서) 추가
- todo.md: GUI 버전 / 인코딩 개선 / Excel 링크 완료 처리
- .gitignore: vcpkg/, do_commit.ps1, test 파일 등 추가
"@

git commit -m $msg

git push origin main
if ($LASTEXITCODE -eq 0) {
    Write-Host "Push 완료!"
} else {
    Write-Host "Push 실패 (exit: $LASTEXITCODE)"
}
