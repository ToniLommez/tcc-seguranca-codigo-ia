@echo off
setlocal enabledelayedexpansion
echo ============================================
echo    Lead Manager - Build
echo ============================================
echo.

set VCPKG_ROOT=C:\vcpkg
set TRIPLET=x64-mingw-dynamic
set MINGW=C:\mingw64\bin

if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    echo [ERRO] vcpkg nao encontrado em %VCPKG_ROOT%
    echo Execute setup.bat primeiro!
    pause
    exit /b 1
)

if not exist "build" mkdir build

echo [INFO] Configurando CMake com MinGW + Ninja...
cmake -S . -B build ^
  -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_C_COMPILER="%MINGW%\gcc.exe" ^
  -DCMAKE_CXX_COMPILER="%MINGW%\g++.exe" ^
  -DCMAKE_MAKE_PROGRAM="%MINGW%\ninja.exe" ^
  -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=%TRIPLET%

if %ERRORLEVEL% NEQ 0 (
    echo [ERRO] Falha na configuracao do CMake
    pause
    exit /b 1
)

echo.
echo [INFO] Compilando...
cmake --build build --config Release --parallel

if %ERRORLEVEL% NEQ 0 (
    echo [ERRO] Falha na compilacao
    pause
    exit /b 1
)

echo.
echo ============================================
echo [OK] Build concluido com sucesso!
echo.
echo Para executar:  build\lead-manager.exe
echo Acesse:         http://localhost:8080
echo ============================================
echo.
pause
