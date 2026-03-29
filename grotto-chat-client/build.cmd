@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "BUILD_DIR=build"
set "BACKUP_DIR=%BUILD_DIR%-old"

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

echo Build completed successfully.
endlocal
