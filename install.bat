@echo off
setlocal
cd /d "%~dp0"
rem Installs Simulacrum for this user: %LOCALAPPDATA%\Programs\Simulacrum, with Start menu and Desktop shortcuts named Atelier - Simulacrum.
rem Run build.bat first (or unzip a release and run this beside it).
set "SRC=dist\Simulacrum"
if exist "Simulacrum.exe" set "SRC=."
if not exist "%SRC%\Simulacrum.exe" ( echo Build it first: build.bat & exit /b 1 )
tasklist /fi "imagename eq Simulacrum.exe" | find /i "Simulacrum.exe" >nul
if not errorlevel 1 ( echo Simulacrum is open: close it and run this again. & exit /b 1 )
set "INST=%LOCALAPPDATA%\Programs\Simulacrum"
if not exist "%INST%" mkdir "%INST%"
xcopy /e /i /q /y "%SRC%\*" "%INST%\" >nul || ( echo Couldn't copy to %INST% & exit /b 1 )
powershell -NoProfile -Command "$s=New-Object -ComObject WScript.Shell; foreach($d in @([Environment]::GetFolderPath('Programs'),[Environment]::GetFolderPath('Desktop'))){ Remove-Item (Join-Path $d 'Simulacrum.lnk') -Force -ErrorAction SilentlyContinue; $l=$s.CreateShortcut((Join-Path $d 'Atelier - Simulacrum.lnk')); $l.TargetPath='%INST%\Simulacrum.exe'; $l.WorkingDirectory='%INST%'; $l.IconLocation='%INST%\Simulacrum.exe,0'; $l.Description='Simulacrum: photos to a textured 3D model'; $l.Save() }"
echo Installed: %INST%
if /i not "%~1"=="--quiet" echo Start it from the Start menu or the Desktop.
exit /b 0
