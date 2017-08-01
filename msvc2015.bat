qbs setup-toolchains --detect
qbs setup-qt  %QTDIR%\bin\qmake.exe Qt
qbs config defaultProfile Qt
qbs generate --generator visualstudio2015 -d _build Debug Release
pause