@echo off
start "" /wait "gpupad.exe" --headless %*
exit /b %ERRORLEVEL%
