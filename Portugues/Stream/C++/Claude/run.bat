@echo off
echo ============================================
echo  StreamMusic - Iniciando servidor
echo ============================================
echo.

if not exist "build\Release\music_server.exe" (
    echo [ERRO] Executavel nao encontrado.
    echo Execute setup.bat e depois build.bat primeiro.
    pause
    exit /b 1
)

cd build\Release
echo Servidor disponivel em: http://localhost:8080
echo Pressione Ctrl+C para parar.
echo.
music_server.exe
