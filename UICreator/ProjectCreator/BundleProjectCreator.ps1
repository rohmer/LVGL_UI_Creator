param([Parameter(Mandatory=$true)][string]$OutputDirectory,[ValidateSet('auto','x64','x86','Win32','x32')][string]$Architecture='auto')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\..\Packaging\VisualStudioHelpers.ps1')
$helperContext=Get-UICreatorVisualStudioHelperContext -OutputDirectory $OutputDirectory -Architecture $Architecture
$repositoryRoot=$helperContext.RepositoryRoot
$destination=Join-Path ([IO.Path]::GetFullPath($OutputDirectory)) 'project-creator'
if($helperContext.Architecture -eq 'x86'){
    $nativeTools=Get-UICreatorVisualStudioX86Tools $helperContext;$python=$nativeTools.projectPython
    Sync-UICreatorVisualStudioHelper -Context $helperContext -Python $python -Source $nativeTools.projectToolDirectory -Destination $destination -Keep @('catalog.json','lvgl-9.6.0.zip','linux-toolkit.zip','pico-toolkit.zip')
}else{
    $environmentDirectory=Join-Path $repositoryRoot 'build\project-tools-env'
    $python=Join-Path $environmentDirectory 'Scripts\python.exe'
    if(!(Test-Path -LiteralPath $python)){
        & python.exe -m venv $environmentDirectory
        if($LASTEXITCODE){throw 'Could not prepare the project helper packaging environment'}
    }
    & $python -c 'import PyInstaller' 2>$null
    if($LASTEXITCODE){
        & $python -m pip install pyinstaller==6.19.0
        if($LASTEXITCODE){throw 'Could not install the project helper packager'}
    }
    $dist=Join-Path $repositoryRoot 'build\project-tool-dist'
    & $python -m PyInstaller --noconfirm --onedir --name UICreatorProjectTool --distpath $dist --workpath (Join-Path $repositoryRoot 'build\project-tool-work') --specpath (Join-Path $repositoryRoot 'build') (Join-Path $PSScriptRoot 'project_creator.py')
    if($LASTEXITCODE){throw 'Project creator packaging failed'}
    New-Item -ItemType Directory -Force -Path $destination | Out-Null
    Get-ChildItem -LiteralPath (Join-Path $dist 'UICreatorProjectTool') -Force | ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination $destination -Recurse -Force}
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'catalog.json') -Destination $destination -Force
& $python (Join-Path $PSScriptRoot 'package_lvgl.py') (Join-Path $repositoryRoot '3rdParty\LVGL\lvgl') (Join-Path $destination 'lvgl-9.6.0.zip')
if($LASTEXITCODE){throw 'LVGL source packaging failed'}
$linuxToolkit = Join-Path $repositoryRoot 'build\project-creator-linux-toolkit'
& $python (Join-Path $repositoryRoot 'Runtime\linux\package_toolkit.py') --output $linuxToolkit --replace
if($LASTEXITCODE){throw 'Linux source toolkit packaging failed'}
Copy-Item -LiteralPath ($linuxToolkit + '.zip') -Destination (Join-Path $destination 'linux-toolkit.zip') -Force
$picoToolkit = Join-Path $repositoryRoot 'build\project-creator-pico-toolkit'
& $python (Join-Path $repositoryRoot 'Runtime\pico\package_toolkit.py') --output $picoToolkit --replace
if($LASTEXITCODE){throw 'Pico source toolkit packaging failed'}
Copy-Item -LiteralPath ($picoToolkit + '.zip') -Destination (Join-Path $destination 'pico-toolkit.zip') -Force
