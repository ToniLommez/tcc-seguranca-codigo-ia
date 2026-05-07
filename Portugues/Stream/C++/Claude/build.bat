@echo off
setlocal

echo ============================================
echo  StreamMusic - Build
echo ============================================
echo.

if not exist build (
    echo [ERRO] Pasta 'build' nao encontrada. Execute setup.bat primeiro.
    pause
    exit /b 1
)

echo Compilando...
cmake --build build --config Release
if errorlevel 1 (
    echo.
    echo [ERRO] Falha na compilacao.
    pause
    exit /b 1
)

echo.
echo ============================================
echo  Build concluido!
echo  Executavel: build\Release\music_server.exe
echo  Execute: run.bat
echo ============================================
pause
