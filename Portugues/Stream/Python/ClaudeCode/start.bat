@echo off
chcp 65001 >nul
title SoundStream

echo.
echo  ==========================================
echo   SoundStream - Iniciando sistema...
echo  ==========================================
echo.

cd /d "%~dp0backend"

echo [1/2] Instalando dependencias Python...
pip install -r requirements.txt --quiet
if errorlevel 1 (
    echo ERRO: Falha ao instalar dependencias.
    pause
    exit /b 1
)

echo [2/2] Iniciando servidor na porta 8000...
echo.
echo  Acesse: http://localhost:8000
echo  Documentacao da API: http://localhost:8000/api/docs
echo.
echo  Pressione Ctrl+C para encerrar.
echo.

uvicorn main:app --host 0.0.0.0 --port 8000 --reload

pause
