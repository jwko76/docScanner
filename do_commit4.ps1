Set-Location "D:\Work\AI\ClaudeCode\PiiScanner"

git add .

$msg = @"
feat: GUI에 실시간 결과 그리드 + 파일/폴더 열기 기능 추가

- main_ui.cpp: TabControl(로그/스캔결과) + ListView 결과 그리드 추가
- 스캔 중 탐지 즉시 행 삽입 (파일명, 유형, 탐지값, 마스킹, 줄번호, 맥락)
- 행 더블클릭 → 파일 바로 열기
- 행 우클릭 → '파일 열기' / '폴더 열기(탐색기 선택 표시)'
- 스캔 완료 시 결과 있으면 자동으로 '스캔 결과' 탭 전환
- 창 너비 700 → 960px 확장
"@

git commit -m $msg

git push origin main
if ($LASTEXITCODE -eq 0) {
    Write-Host "Push 완료!"
} else {
    Write-Host "Push 실패 (exit: $LASTEXITCODE)"
}
