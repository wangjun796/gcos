@echo off
echo ========================================
echo Running All GCOS VM Tests
echo ========================================
echo.

set TEST_DIR=build\Debug
set PASS_COUNT=0
set FAIL_COUNT=0

echo [1/18] test_basic.exe...
call %TEST_DIR%\test_basic.exe > test_basic.log 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   ✓ PASSED
    set /a PASS_COUNT+=1
) else (
    echo   ✗ FAILED
    set /a FAIL_COUNT+=1
)

echo [2/18] test_app_manager.exe...
call %TEST_DIR%\test_app_manager.exe > test_app_manager.log 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   ✓ PASSED
    set /a PASS_COUNT+=1
) else (
    echo   ✗ FAILED
    set /a FAIL_COUNT+=1
)

echo [3/18] test_aid_prefix_match.exe...
call %TEST_DIR%\test_aid_prefix_match.exe > test_aid_prefix_match.log 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   ✓ PASSED
    set /a PASS_COUNT+=1
) else (
    echo   ✗ FAILED
    set /a FAIL_COUNT+=1
)

echo [4/18] test_app_metadata.exe...
call %TEST_DIR%\test_app_metadata.exe > test_app_metadata.log 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   ✓ PASSED
    set /a PASS_COUNT+=1
) else (
    echo   ✗ FAILED
    set /a FAIL_COUNT+=1
)

echo [5/18] test_select_command.exe...
call %TEST_DIR%\test_select_command.exe > test_select_command.log 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   ✓ PASSED
    set /a PASS_COUNT+=1
) else (
    echo   ✗ FAILED
    set /a FAIL_COUNT+=1
)

echo [6/18] test_load_command.exe...
call %TEST_DIR%\test_load_command.exe > test_load_command.log 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   ✓ PASSED
    set /a PASS_COUNT+=1
) else (
    echo   ✗ FAILED
    set /a FAIL_COUNT+=1
)

echo [7/18] test_module_registry.exe...
call %TEST_DIR%\test_module_registry.exe > test_module_registry.log 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   ✓ PASSED
    set /a PASS_COUNT+=1
) else (
    echo   ✗ FAILED
    set /a FAIL_COUNT+=1
)

echo [8/18] test_symbol_resolver.exe...
call %TEST_DIR%\test_symbol_resolver.exe > test_symbol_resolver.log 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   ✓ PASSED
    set /a PASS_COUNT+=1
) else (
    echo   ✗ FAILED
    set /a FAIL_COUNT+=1
)

echo [9/18] test_install_command.exe...
call %TEST_DIR%\test_install_command.exe > test_install_command.log 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   ✓ PASSED
    set /a PASS_COUNT+=1
) else (
    echo   ✗ FAILED
    set /a FAIL_COUNT+=1
)

echo [10/18] test_delete_command.exe...
call %TEST_DIR%\test_delete_command.exe > test_delete_command.log 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   ✓ PASSED
    set /a PASS_COUNT+=1
) else (
    echo   ✗ FAILED
    set /a FAIL_COUNT+=1
)

echo [11/18] test_app_delete_simple.exe...
call %TEST_DIR%\test_app_delete_simple.exe > test_app_delete_simple.log 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   ✓ PASSED
    set /a PASS_COUNT+=1
) else (
    echo   ✗ FAILED
    set /a FAIL_COUNT+=1
)

echo [12/18] test_app_delete_grt_cleanup.exe...
call %TEST_DIR%\test_app_delete_grt_cleanup.exe > test_app_delete_grt_cleanup.log 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   ✓ PASSED
    set /a PASS_COUNT+=1
) else (
    echo   ✗ FAILED
    set /a FAIL_COUNT+=1
)

echo [13/18] test_load_module_registry.exe...
call %TEST_DIR%\test_load_module_registry.exe > test_load_module_registry.log 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   ✓ PASSED
    set /a PASS_COUNT+=1
) else (
    echo   ✗ FAILED
    set /a FAIL_COUNT+=1
)

echo [14/18] test_persistence.exe...
call %TEST_DIR%\test_persistence.exe > test_persistence.log 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   ✓ PASSED
    set /a PASS_COUNT+=1
) else (
    echo   ✗ FAILED
    set /a FAIL_COUNT+=1
)

echo [15/18] test_sef_parsing.exe...
call %TEST_DIR%\test_sef_parsing.exe > test_sef_parsing.log 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   ✓ PASSED
    set /a PASS_COUNT+=1
) else (
    echo   ✗ FAILED
    set /a FAIL_COUNT+=1
)

echo [16/18] test_generated_sef.exe...
call %TEST_DIR%\test_generated_sef.exe > test_generated_sef.log 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   ✓ PASSED
    set /a PASS_COUNT+=1
) else (
    echo   ✗ FAILED
    set /a FAIL_COUNT+=1
)

echo [17/18] test_gcos_vm_simple.exe...
call %TEST_DIR%\test_gcos_vm_simple.exe > test_gcos_vm_simple.log 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   ✓ PASSED
    set /a PASS_COUNT+=1
) else (
    echo   ✗ FAILED
    set /a FAIL_COUNT+=1
)

echo.
echo ========================================
echo Test Summary
echo ========================================
echo Total: 17
echo Passed: %PASS_COUNT%
echo Failed: %FAIL_COUNT%
echo.

if %FAIL_COUNT% EQU 0 (
    echo ✓ ALL TESTS PASSED!
    exit /b 0
) else (
    echo ✗ SOME TESTS FAILED
    exit /b 1
)
