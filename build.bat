@echo off
rem Compile RocketDock Modern sous Windows avec Zig (aucun Visual Studio requis).
rem Prerequis : Python 3 puis   pip install ziglang
cd /d "%~dp0"
if not exist build mkdir build
set FLAGS=-O2 -std=c++17 -static -s -Wl,--subsystem,windows src\main.cpp resources\app.rc
set LIBS=-lgdiplus -lgdi32 -luser32 -lshell32 -lole32 -luuid -ldwmapi -lcomdlg32 -lshlwapi -ladvapi32 -luxtheme -lwinmm -lcomctl32 -lwinhttp
python -m ziglang c++ -target x86_64-windows-gnu %FLAGS% -o build\RocketDockModern_x64.exe %LIBS% || goto :err
python -m ziglang c++ -target x86-windows-gnu    %FLAGS% -o build\RocketDockModern_x86.exe %LIBS% || goto :err
echo OK : executables dans le dossier build
goto :eof
:err
echo ECHEC de la compilation
exit /b 1
