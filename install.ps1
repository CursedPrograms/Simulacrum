# Installs Simulacrum for this user (%LOCALAPPDATA%\Programs\Simulacrum, Start menu and Desktop shortcuts named "Atelier - Simulacrum"), through install.bat.
& cmd /c "`"$PSScriptRoot\install.bat`" $args"
exit $LASTEXITCODE
