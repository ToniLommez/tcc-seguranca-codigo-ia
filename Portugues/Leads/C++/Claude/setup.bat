@echo off
setlocal enabledelayedexpansion
echo ============================================
echo    Lead Manager - Setup de Dependencias
echo ============================================
echo.

:: Verificar se vcpkg esta instalado
set VCPKG_ROOT=C:\vcpkg
where vcpkg >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [OK] vcpkg encontrado no PATH
    goto :install_deps
)

if exist "%VCPKG_ROOT%\vcpkg.exe" (
    echo [OK] vcpkg encontrado em %VCPKG_ROOT%
    set PATH=%VCPKG_ROOT%;%PATH%
    goto :install_deps
)

echo [INFO] Instalando vcpkg em C:\vcpkg...
cd /d C:\
git clone https://github.com/microsoft/vcpkg.git
if %ERRORLEVEL% NEQ 0 (
    echo [ERRO] Falha ao clonar vcpkg. Verifique se o Git esta instalado.
    pause
    exit /b 1
)
cd vcpkg
call bootstrap-vcpkg.bat -disableMetrics
vcpkg integrate install
set PATH=C:\vcpkg;%PATH%
echo [OK] vcpkg instalado com sucesso
cd /d %~dp0

:install_deps
echo.
echo [INFO] Instalando dependencias (pode demorar alguns minutos)...
echo.

vcpkg install crow:x64-windows
vcpkg install nlohmann-json:x64-windows
vcpkg install curl:x64-windows
vcpkg install openssl:x64-windows

echo.
echo [OK] Dependencias instaladas!
echo.
echo Proximo passo: execute build.bat para compilar o projeto
echo.
pause
