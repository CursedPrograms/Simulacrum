@echo off
setlocal
cd /d "%~dp0"
rem Builds Simulacrum (Release), dist\Simulacrum-win64.zip (portable) and dist\Simulacrum-setup.exe (the installer).
rem Needs Visual Studio 2022 (C++), CMake, Python (for the runtime) and Inno Setup 6 (for the installer).
rem The AliceVision runtime comes from Meshroom 2025.1's aliceVision folder: set MESHROOM_AV to it (see README).
if not defined MESHROOM_AV set "MESHROOM_AV=D:\cc\_src\meshroom-bin\Meshroom-2025.1.0\aliceVision"
if not defined BUILD_DIR set "BUILD_DIR=build"
if not exist "external\SDL2-2.30.9\cmake" (
    echo Getting SDL2's development package, once...
    if not exist external mkdir external
    curl -sL -o external\SDL2-devel.zip https://github.com/libsdl-org/SDL/releases/download/release-2.30.9/SDL2-devel-2.30.9-VC.zip || goto :error
    tar -xf external\SDL2-devel.zip -C external || goto :error
    del external\SDL2-devel.zip
)
if not exist "runtime\alicevision-min\bin\aliceVision_meshing.exe" (
    echo Putting the AliceVision runtime together, once...
    python tools\make_runtime.py "%MESHROOM_AV%" runtime\alicevision-min || goto :error
)
cmake -B %BUILD_DIR% -G "Visual Studio 17 2022" -A x64 || goto :error
cmake --build %BUILD_DIR% --config Release || goto :error
if exist dist\Simulacrum rmdir /s /q dist\Simulacrum
mkdir dist\Simulacrum
copy /y %BUILD_DIR%\Release\Simulacrum.exe dist\Simulacrum\ >nul || goto :error
copy /y %BUILD_DIR%\Release\SDL2.dll dist\Simulacrum\ >nul || goto :error
xcopy /e /i /q /y %BUILD_DIR%\Release\assets dist\Simulacrum\assets >nul
xcopy /e /i /q /y runtime\alicevision-min dist\Simulacrum\alicevision >nul || goto :error
rem Microsoft's Visual C++ runtime and OpenMP, beside AliceVision's programs (PCs without the redistributable)
set "CRT="
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -find VC\Redist\MSVC\*\x64\Microsoft.VC143.CRT`) do set "CRT=%%i"
if not defined CRT echo Visual C++ runtime folder not found, is Visual Studio 2022 installed?& goto :error
xcopy /q /y "%CRT%\*.dll" dist\Simulacrum\alicevision\bin\ >nul || goto :error
for %%i in ("%CRT%\..\Microsoft.VC143.OpenMP\vcomp140.dll") do if exist %%i copy /y %%i dist\Simulacrum\alicevision\bin\ >nul
for %%f in (README.md LICENSE NOTICE install.bat install.ps1) do if exist %%f copy /y %%f dist\Simulacrum\ >nul
if exist dist\Simulacrum-win64.zip del dist\Simulacrum-win64.zip
"%SystemRoot%\System32\tar.exe" -a -c -f dist\Simulacrum-win64.zip -C dist Simulacrum || goto :error
set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if exist "%ISCC%" ( "%ISCC%" /Q installer\simulacrum.iss || goto :error ) else echo Inno Setup 6 isn't installed: no installer, the zip is there.
echo Built: dist\Simulacrum-win64.zip and dist\Simulacrum-setup.exe
rem installed on this PC: this build replaces that copy too
if exist "%LOCALAPPDATA%\Programs\Simulacrum\Simulacrum.exe" call "%~dp0install.bat" --quiet
exit /b 0
:error
echo Build failed, see above.
pause
exit /b 1
