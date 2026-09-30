@echo off
set "APP=%~dp0groupsync_gui.exe"
if not exist "%APP%" (
	echo GroupSync GUI is not built. Run the "GroupSync: Build Native GUI" task first.
	exit /b 1
)
start "" /D "%~dp0" "%APP%"