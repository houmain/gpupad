@echo off
start "" /wait "gpupad.exe" --mcp %*
exit /b %ERRORLEVEL%
