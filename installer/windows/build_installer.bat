@echo off
REM ===========================================================================
REM  build_installer.bat — one-shot Windows build + installer package.
REM
REM  Run from the repository root in a "Developer Command Prompt for VS 2022"
REM  (so CMake finds MSVC):
REM      installer\windows\build_installer.bat
REM
REM  Requires: CMake, Visual Studio 2022 (x64), and Inno Setup 6
REM  (https://jrsoftware.org/isinfo.php). Network access is needed once so
REM  CMake can fetch JUCE.
REM ===========================================================================
setlocal

echo [1/3] Configuring...
cmake -B build -G "Visual Studio 17 2022" -A x64 -DREDLINE_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 goto :fail

echo [2/3] Building plugin (Release, x64)...
cmake --build build --config Release --target Redline120_All --parallel
if errorlevel 1 goto :fail

echo [3/3] Packaging installer...
set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" (
    echo ERROR: Inno Setup 6 not found at "%ISCC%".
    echo Install it from https://jrsoftware.org/isinfo.php or "choco install innosetup".
    goto :fail
)
"%ISCC%" "installer\windows\Redline120.iss"
if errorlevel 1 goto :fail

echo.
echo Done. Installer is in installer\windows\Output\
exit /b 0

:fail
echo.
echo Build failed.
exit /b 1
