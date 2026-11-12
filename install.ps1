# Installs Terminus for this user (%LOCALAPPDATA%\Programs\Terminus, Start menu + Desktop shortcuts), through install.bat.
& cmd /c "`"$PSScriptRoot\install.bat`" $args"
exit $LASTEXITCODE
