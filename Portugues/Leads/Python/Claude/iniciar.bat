@echo off
title Lead Manager

echo ================================================
echo   Lead Manager - Claude + Python + Firebase
echo ================================================
echo.

cd /d "%~dp0backend"

if not exist ".venv" (
    echo [1/3] Criando ambiente virtual...
    python -m venv .venv
)

echo [2/3] Ativando ambiente e instalando dependencias...
call .venv\Scripts\activate.bat
pip install -r requirements.txt --quiet

echo [3/3] Iniciando servidor...
echo.
echo  Acesse: http://localhost:8000
echo  Para encerrar: Ctrl+C
echo.

python main.py

pause
