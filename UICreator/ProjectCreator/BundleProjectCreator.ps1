param([Parameter(Mandatory=$true)][string]$OutputDirectory,[ValidateSet('auto','x64','x86','Win32','x32')][string]$Architecture='auto')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\..\Packaging\VisualStudioHelpers.ps1')
$helperContext=Get-UICreatorVisualStudioHelperContext -OutputDirectory $OutputDirectory -Architecture $Architecture
$repositoryRoot=$helperContext.RepositoryRoot
$destination=Join-Path ([IO.Path]::GetFullPath($OutputDirectory)) 'project-creator'
if($helperContext.Architecture -eq 'x86'){
    $nativeTools=Get-UICreatorVisualStudioX86Tools $helperContext;$python=$nativeTools.imagePython
    Sync-UICreatorVisualStudioHelper -Context $helperContext -Python $python -Source $nativeTools.projectToolDirectory -Destination $destination -Keep @('catalog.json','lvgl-9.6.0.zip','linux-toolkit.zip')
}else{
    $python=Join-Path $repositoryRoot 'build\image-tools-env\Scripts\python.exe'
    if(!(Test-Path -LiteralPath $python)){throw 'Run BundleImageConverter.ps1 first to prepare the packaging environment'}
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
