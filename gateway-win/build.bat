@echo off
REM ============================================
REM Windows Gateway Build Script
REM ============================================

echo ====================================================
echo   Building Windows Gateway
echo ====================================================

REM Create build directory
if not exist build mkdir build
cd build

REM Run CMake
echo [1/2] Running CMake...
cmake -G "Visual Studio 17 2022" -A x64 ..
if errorlevel 1 (
    echo CMake configuration failed!
    echo Trying with Visual Studio 2019...
    cmake -G "Visual Studio 16 2019" -A x64 ..
    if errorlevel 1 (
        echo CMake failed. Make sure Visual Studio is installed.
        pause
        exit /b 1
    )
)

REM Build
echo [2/2] Building...
cmake --build . --config Release
if errorlevel 1 (
    echo Build failed!
    pause
    exit /b 1
)

echo.
echo ====================================================
echo   Build Successful!
echo ====================================================
echo.
echo To run the Gateway:
echo   cd build\bin\Release
echo   GatewayWin.exe 8888 --discover
echo.
echo Or with manual backend:
echo   GatewayWin.exe 8888 127.0.0.1:9091
echo.
pause
