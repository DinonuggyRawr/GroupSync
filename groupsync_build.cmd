@echo off
setlocal
set "ROOT=%~dp0"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo Visual Studio Installer's vswhere.exe was not found.
    exit /b 1
)
for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%I"
if not defined VSROOT (
    echo Visual Studio C++ Build Tools were not found.
    exit /b 1
)
if not exist "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" (
    echo The Visual Studio x64 compiler environment was not found.
    exit /b 1
)
call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
pushd "%ROOT%"
if not defined GROUPSYNC_GUI_OUTPUT set "GROUPSYNC_GUI_OUTPUT=groupsync_gui.exe"
cl.exe /nologo /W4 /std:c11 /Fe:"%GROUPSYNC_GUI_OUTPUT%" groupsync_gui.c availabilitymatcher.c /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib comctl32.lib shell32.lib ole32.lib windowscodecs.lib msimg32.lib
if errorlevel 1 (
    popd
    exit /b 1
)
cl.exe /nologo /W4 /TC /Fe:groupsync.exe main.c availabilitymatcher.c
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
