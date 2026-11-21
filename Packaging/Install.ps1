param([string]$InstallDirectory = (Join-Path $env:LOCALAPPDATA 'Programs\LVGL UI Creator\0.9.0'))
$ErrorActionPreference = 'Stop'
$packageDirectory = $PSScriptRoot
if (-not (Test-Path -LiteralPath (Join-Path $packageDirectory 'UICreator.exe'))) {
    throw 'Run Install.ps1 from an extracted release package.'
}
$targetDirectory = [IO.Path]::GetFullPath($InstallDirectory)
$packageFullPath = [IO.Path]::GetFullPath($packageDirectory).TrimEnd('\')
if ($targetDirectory.TrimEnd('\') -eq $packageFullPath -or $targetDirectory.StartsWith($packageFullPath + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Choose an install directory outside the extracted package.'
}
New-Item -ItemType Directory -Force -Path $targetDirectory | Out-Null
Get-ChildItem -LiteralPath $packageDirectory | Copy-Item -Destination $targetDirectory -Recurse -Force
$shell = New-Object -ComObject WScript.Shell
$programsDirectory = [Environment]::GetFolderPath('Programs')
$shortcut = $shell.CreateShortcut((Join-Path $programsDirectory 'LVGL UI Creator.lnk'))
$shortcut.TargetPath = Join-Path $targetDirectory 'UICreator.exe'
$shortcut.WorkingDirectory = $targetDirectory
$shortcut.IconLocation = Join-Path $targetDirectory 'UICreator.ico'
$shortcut.Save()
Write-Output "Installed LVGL UI Creator 0.9.0 in $targetDirectory"
Write-Output 'Open LVGL UI Creator from the Start menu. Project and recovery data stay in your user profile.'
