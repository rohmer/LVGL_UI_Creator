param([string]$ApplicationDirectory='', [string]$Project='', [switch]$KeepOpen)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(!$ApplicationDirectory){$ApplicationDirectory=Join-Path $repo 'build\windows-installer\payload\x64'}
$ApplicationDirectory=[IO.Path]::GetFullPath($ApplicationDirectory)
if(!(Test-Path -LiteralPath (Join-Path $ApplicationDirectory 'architecture-manifest.json'))){throw 'Launch requires a verified application payload from BuildWindows.ps1.'}
$manifest=Get-Content -LiteralPath (Join-Path $ApplicationDirectory 'architecture-manifest.json') -Raw | ConvertFrom-Json
if($manifest.architecture -notin @('x64','x86')){throw 'Invalid payload architecture.'}
if($manifest.cmakeExecutable -and (Test-Path -LiteralPath $manifest.cmakeExecutable -PathType Leaf)){
    $env:Path=(Split-Path $manifest.cmakeExecutable -Parent)+[IO.Path]::PathSeparator+$env:Path
}
$live=Join-Path $repo ('build\editor-live-'+[guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $live | Out-Null
foreach($name in @('UICreator.exe','UICreatorViewer.exe','UICreatorCodeGen.exe','LVGLCompiler.exe')){
    Copy-Item -LiteralPath (Join-Path $ApplicationDirectory $name) -Destination $live
}
Get-ChildItem -LiteralPath $ApplicationDirectory -Filter '*.dll' -File | Copy-Item -Destination $live
foreach($name in @('font-converter','image-converter','project-creator','language-models','docs','licenses','sdk')){
    $source=Join-Path $ApplicationDirectory $name
    if(Test-Path -LiteralPath $source){Copy-Item -LiteralPath $source -Destination $live -Recurse}
}
Copy-Item -LiteralPath (Join-Path $ApplicationDirectory 'UICreator.ico') -Destination $live
$env:UICREATOR_SESSION_DIR=Join-Path $live 'session'
Remove-Item Env:SDL_VIDEO_DRIVER -ErrorAction SilentlyContinue
Remove-Item Env:SDL_RENDER_DRIVER -ErrorAction SilentlyContinue
$launch=@{FilePath=(Join-Path $live 'UICreator.exe');WorkingDirectory=$live;WindowStyle='Normal';PassThru=$true}
if($Project){$launch.ArgumentList='"'+[IO.Path]::GetFullPath($Project)+'"'}
$process=Start-Process @launch
Start-Sleep -Seconds 3
$process.Refresh()
if($process.HasExited){throw 'The updated app exited during startup.'}
$state=@{processId=$process.Id;architecture=$manifest.architecture;liveDirectory=$live;project=$Project;responding=$process.Responding}
$state | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $repo 'build\latest-polish-launch.json') -Encoding UTF8
$state | ConvertTo-Json
if($KeepOpen){Wait-Process -Id $process.Id}
