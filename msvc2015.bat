
PATH=%PATH%;%QTDIR%\bin
PATH=%PATH%;%QTDIR%\..\..\Tools\QtCreator\bin

qbs-setup-toolchains --detect
qbs-setup-qt %QTDIR%\bin\qmake.exe qt5
qbs-config profiles.qt5.baseProfile MSVC2015-amd64
qbs generate --generator visualstudio2015 -d _build profile:qt5 Debug Release

pause
