# Builds Terminus (Release), dist\Terminus-win64.zip and dist\Terminus-setup.exe, through build.bat.
$ErrorActionPreference = 'Stop'
& cmd /c "`"$PSScriptRoot\build.bat`""
exit $LASTEXITCODE
