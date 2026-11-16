# Installs Simulacrum for this user (%LOCALAPPDATA%\Programs\Simulacrum, Start menu + Desktop shortcuts), through install.bat.
& cmd /c "`"$PSScriptRoot\install.bat`" $args"
exit $LASTEXITCODE
