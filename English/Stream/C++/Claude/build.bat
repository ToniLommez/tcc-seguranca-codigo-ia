@echo off
setlocal

echo === SoundStream Build Script ===
echo.

:: Check if vcpkg is available
where vcpkg >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] vcpkg not found in PATH.
    echo.
    echo Please install vcpkg:
    echo   1. git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
    echo   2. cd C:\vcpkg
    echo   3. .\bootstrap-vcpkg.bat
    echo   4. Add C:\vcpkg to your PATH
    echo   5. Run: vcpkg integrate install
    echo.
    pause
    exit /b 1
)

:: Get vcpkg root
for /f "delims=" %%i in ('where vcpkg') do set VCPKG_EXE=%%i
for %%i in ("%VCPKG_EXE%") do set VCPKG_ROOT=%%~dpi

echo [1/3] Installing dependencies via vcpkg...
vcpkg install cpp-httplib nlohmann-json jwt-cpp openssl curl --triplet x64-windows
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to install dependencies.
    pause
    exit /b 1
)

echo.
echo [2/3] Configuring with CMake...
if not exist build mkdir build
cd build

cmake .. -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake configuration failed.
    pause
    exit /b 1
)

echo.
echo [3/3] Building...
cmake --build . --config Release
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Build failed.
    pause
    exit /b 1
)

echo.
echo === Build Complete ===
echo.
echo To run the server:
echo   cd build\Release
echo   soundstream.exe
echo.
echo Then open http://localhost:8080 in your browser.
echo.
pause
