Set-Location "D:\Work\AI\ClaudeCode\PiiScanner"

git add .

$msg = @"
feat: HTML/Excel 파일 목록에 탐지 유형 컬럼 추가

- reporter.cpp: 파일 목록 탭에 '탐지 유형' 배지 컬럼 추가 (HTML)
- reporter.cpp: 파일 목록 시트에 '탐지 유형' 텍스트 컬럼 추가 (Excel)
- typeBadge 람다를 파일 목록/상세 결과 공통 사용 가능하도록 위치 이동
- 파일별 고유 PII 유형을 중복 없이 열거 (전화번호, 주민등록번호 등)
"@

git commit -m $msg

git push origin main
if ($LASTEXITCODE -eq 0) {
    Write-Host "Push 완료!"
} else {
    Write-Host "Push 실패 (exit: $LASTEXITCODE)"
}
