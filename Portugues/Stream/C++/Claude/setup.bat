@echo off
setlocal

echo ============================================
echo  StreamMusic - Setup de Dependencias
echo ============================================
echo.

REM --- Verifica se vcpkg esta configurado ---
if "%VCPKG_ROOT%"=="" (
    echo [AVISO] Variavel VCPKG_ROOT nao definida.
    echo.
    echo Opcoes:
    echo   1) Se vcpkg ja esta instalado, defina a variavel:
    echo      set VCPKG_ROOT=C:\caminho\para\vcpkg
    echo      e execute este script novamente.
    echo.
    echo   2) Para instalar vcpkg agora, execute:
    echo      git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
    echo      C:\vcpkg\bootstrap-vcpkg.bat
    echo      set VCPKG_ROOT=C:\vcpkg
    echo      e execute este script novamente.
    echo.
    pause
    exit /b 1
)

echo [OK] VCPKG_ROOT = %VCPKG_ROOT%
echo.

echo [1/3] Instalando dependencias via vcpkg...
%VCPKG_ROOT%\vcpkg.exe install openssl:x64-windows curl:x64-windows sqlite3:x64-windows nlohmann-json:x64-windows cpp-httplib:x64-windows
if errorlevel 1 (
    echo [ERRO] Falha ao instalar dependencias.
    pause
    exit /b 1
)

echo.
echo [2/3] Integrando vcpkg com o sistema...
%VCPKG_ROOT%\vcpkg.exe integrate install

echo.
echo [3/3] Configurando projeto CMake...
if not exist build mkdir build
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows
if errorlevel 1 (
    echo [ERRO] Falha na configuracao do CMake.
    pause
    exit /b 1
)

echo.
echo ============================================
echo  Setup concluido! Execute build.bat
echo ============================================
pause
