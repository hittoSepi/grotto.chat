@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "BUILD_DIR=build"
set "BACKUP_DIR=%BUILD_DIR%-old"
set "RUN_CHECK=0"
set "RUN_QA=0"

if /I "%~1"=="check" set "RUN_CHECK=1"
if /I "%~1"=="test" set "RUN_CHECK=1"
if /I "%~1"=="qa" (
    set "RUN_CHECK=1"
    set "RUN_QA=1"
)

if exist "%BUILD_DIR%" (
    set /a IDX=1
    :find_backup
    if exist "!BACKUP_DIR!" (
        set "BACKUP_DIR=%BUILD_DIR%-old-!IDX!"
        set /a IDX+=1
        goto find_backup
    )

    echo Renaming "%BUILD_DIR%" to "!BACKUP_DIR!"...
    move "%BUILD_DIR%" "!BACKUP_DIR!" >nul
    if errorlevel 1 (
        echo Failed to rename "%BUILD_DIR%".
        exit /b 1
    )
)

echo Configuring CMake...
cmake -S . -B "%BUILD_DIR%"
if errorlevel 1 exit /b 1

echo Building Release...
cmake --build "%BUILD_DIR%" --config Release
if errorlevel 1 exit /b 1

if "%RUN_CHECK%"=="1" (
    echo Running Release test check...
    cmake --build "%BUILD_DIR%" --config Release --target check
    if errorlevel 1 exit /b 1
)

if "%RUN_QA%"=="1" (
    echo Preparing error-scenario QA workspace...
    cmake --build "%BUILD_DIR%" --config Release --target qa-error-scenarios
    if errorlevel 1 exit /b 1
)

echo Build completed successfully.
endlocal
