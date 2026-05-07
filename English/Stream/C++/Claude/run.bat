@echo off
echo === Starting SoundStream Server ===
echo.

:: Copy required files to build/Release directory
if exist build\Release (
    copy /Y firebase-credentials.json build\Release\ >nul 2>&1
    xcopy /E /I /Y frontend build\Release\frontend >nul 2>&1
    if not exist build\Release\uploads mkdir build\Release\uploads
    cd build\Release
    soundstream.exe
) else if exist build\Debug (
    copy /Y firebase-credentials.json build\Debug\ >nul 2>&1
    xcopy /E /I /Y frontend build\Debug\frontend >nul 2>&1
    if not exist build\Debug\uploads mkdir build\Debug\uploads
    cd build\Debug
    soundstream.exe
) else (
    echo [ERROR] Build not found. Run build.bat first.
    pause
)
