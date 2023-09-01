@echo off
setlocal ENABLEDELAYEDEXPANSION

@REM This script is intended to be used either by the CI or by a developer that
@REM wants to check that no unit tests have been broken to ensure there are no
@REM regressions. At the end, it reports how many tests were successful.

set SCRIPT_DIR=%~dp0
set GRAVITY_BIN=%SCRIPT_DIR%/../../gravity_visualstudio/bin/cli/x64/Debug/gravity.exe
set /a tests_total=0
set /a tests_success=0
set /a tests_fail=0
set /a tests_timeout=0
set /a i=1

for /f %%A in ('dir /s %SCRIPT_DIR%\*.gravity ^| find "File(s)"') do set tests_total=%%A

for /f "delims=" %%A in ('dir /s /b %SCRIPT_DIR%\*.gravity') do (
    set test=%%A
    echo "Testing !i!/%tests_total% - !test!..."
    "%GRAVITY_BIN%" "!test!"
    set res=!ERRORLEVEL!
    if !res! EQU 0 (
        set /a tests_success+=1
        echo "Success!"
    ) else (
        echo "Fail!"
        set /a tests_fail+=1
    )
    set /a i+=1
)

echo "Tests run successfully: %tests_success%/%tests_total%. %tests_fail% failed and %tests_timeout% timed out"

set /a tests_non_success=%tests_fail%+%tests_timeout%
if %tests_non_success% NEQ 0 (
    exit 1
)
