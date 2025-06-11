@echo off
setlocal

echo ========================================
echo Building Tray Controls for foobar2000
echo ========================================

REM Clean previous build artifacts first
echo Cleaning previous build artifacts...
call clean-build.bat >nul 2>&1

REM Set the project file path
set PROJECT_FILE=foo_traycontrols.vcxproj

REM Check if the project file exists
if not exist "%PROJECT_FILE%" (
    echo Error: Project file "%PROJECT_FILE%" not found.
    echo Make sure you're running this from the correct directory.
    pause
    exit /b 1
)

REM Check if foobar2000 SDK exists
if not exist "lib\foobar2000_SDK" (
    echo Error: lib\foobar2000_SDK directory not found.
    echo Make sure the SDK is properly extracted in the lib directory.
    pause
    exit /b 1
)

REM Build the project for x64 Release
echo Building %PROJECT_FILE% for x64 Release...
echo.
msbuild "%PROJECT_FILE%" /p:Configuration=Release /p:Platform=x64 /v:minimal

REM Check if the build was successful
if %ERRORLEVEL% neq 0 (
    echo.
    echo ========================================
    echo Build failed with error code %ERRORLEVEL%.
    echo ========================================
    echo.
    echo Common issues:
    echo - Make sure Visual Studio Build Tools are installed
    echo - Check that foobar2000 SDK paths are correct
    echo - Ensure all required dependencies are available
    echo - Note: This component does NOT require ATL/MFC libraries
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo ========================================
echo Build completed successfully!
echo ========================================
echo.

REM Check if the output DLL was created
if exist "x64\Release\foo_traycontrols.dll" (
    echo Output: x64\Release\foo_traycontrols.dll
    echo.
    echo To install:
    echo 1. Copy foo_traycontrols.dll to your foobar2000\components folder
    echo 2. Restart foobar2000
    echo 3. The tray controls will be automatically active
) else (
    echo Warning: Expected output file not found at x64\Release\foo_traycontrols.dll
)

echo.
pause