@echo off
setlocal
cd /d "%~dp0"
rem Installs Terminus for this user: %LOCALAPPDATA%\Programs\Terminus, with Start menu and Desktop shortcuts.
rem Run build.bat first (or unzip a release and run this beside it).
set "SRC=dist\Terminus"
if exist "Terminus.exe" set "SRC=."
if not exist "%SRC%\Terminus.exe" ( echo Build it first: build.bat & exit /b 1 )
tasklist /fi "imagename eq Terminus.exe" | find /i "Terminus.exe" >nul
if not errorlevel 1 ( echo Terminus is open: close it and run this again. & exit /b 1 )
set "INST=%LOCALAPPDATA%\Programs\Terminus"
if not exist "%INST%" mkdir "%INST%"
xcopy /e /i /q /y "%SRC%\*" "%INST%\" >nul || ( echo Couldn't copy to %INST% & exit /b 1 )
powershell -NoProfile -Command "$s=New-Object -ComObject WScript.Shell; foreach($d in @([Environment]::GetFolderPath('Programs'),[Environment]::GetFolderPath('Desktop'))){ $l=$s.CreateShortcut((Join-Path $d 'Terminus.lnk')); $l.TargetPath='%INST%\Terminus.exe'; $l.WorkingDirectory='%INST%'; $l.IconLocation='%INST%\Terminus.exe,0'; $l.Description='Terminus: photos to a textured 3D model'; $l.Save() }"
echo Installed: %INST%
if /i not "%~1"=="--quiet" echo Start it from the Start menu or the Desktop.
exit /b 0
