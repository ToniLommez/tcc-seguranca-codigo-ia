@echo off
echo Starting Lead Manager Frontend...
cd /d "%~dp0frontend"
npx vite --host
pause
