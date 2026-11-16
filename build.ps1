# Builds Simulacrum (Release), dist\Simulacrum-win64.zip and dist\Simulacrum-setup.exe, through build.bat.
$ErrorActionPreference = 'Stop'
& cmd /c "`"$PSScriptRoot\build.bat`""
exit $LASTEXITCODE
