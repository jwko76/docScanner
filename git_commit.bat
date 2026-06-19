@echo off
chcp 65001 > nul
echo.
echo ====================================================
echo   docScanner - 변경사항 커밋 및 Push
echo ====================================================
echo.

git add .
git status

echo.
set /p CONFIRM="위 변경사항을 커밋하시겠습니까? (y/n): "
if /i not "%CONFIRM%"=="y" (
    echo 취소됨.
    pause
    exit /b 0
)

git commit -m "refactor: 보안 설계 강화 - 사용자 폴더 전용 스캔 + 시스템 폴더 제외

- SYSTEM_EXCLUDED_PATHS: Windows/ProgramFiles/ProgramData 등 하드코드 제외
- get_user_default_paths(): winreg 미사용, 환경변수로만 경로 조회
- EverythingScanner.scan(): root_paths 리스트 + exclude_system 옵션 지원
- 기본 스캔 대상 변경: 전체 드라이브 → %%USERPROFILE%% 등 사용자 공간만
- 신규 CLI 옵션: --all-drives, --exclude, --include-system
- run.bat: 스캔 범위 3단계 선택 메뉴 (사용자폴더/특정폴더/전체드라이브)
- 코드 검증: winreg/socket/urllib/requests 본문 미사용 확인
- worklog.md, todo.md 세션 3 기록 업데이트"

git push origin main

if errorlevel 1 (
    echo.
    echo [오류] push 실패. 인증 정보를 확인하세요.
    echo   - Personal Access Token: https://github.com/settings/tokens
) else (
    echo.
    echo ====================================================
    echo   Push 완료!
    echo ====================================================
)
echo.
pause
