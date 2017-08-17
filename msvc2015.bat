
PATH=%PATH%;%QTDIR%\bin
PATH=%PATH%;%QTDIR%\..\..\Tools\QtCreator\bin

qbs-setup-toolchains --detect
qbs-setup-qt %QTDIR%\bin\qmake.exe Qt
qbs-config profiles.Qt.baseProfile MSVC2015-amd64
qbs-config defaultProfile Qt
qbs generate --generator visualstudio2015 -d _build Debug Release
pause
