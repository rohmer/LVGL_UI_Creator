param([Parameter(Mandatory=$true)][string]$OutputDirectory,[ValidateSet('auto','x64','x86','Win32','x32')][string]$Architecture='auto')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\..\Packaging\VisualStudioHelpers.ps1')
$helperContext=Get-UICreatorVisualStudioHelperContext -OutputDirectory $OutputDirectory -Architecture $Architecture
$repositoryRoot=$helperContext.RepositoryRoot
if($helperContext.Architecture -eq 'x86'){
    $nativeTools=Get-UICreatorVisualStudioX86Tools $helperContext
    $destination=Join-Path $helperContext.OutputDirectory 'image-converter'
    Sync-UICreatorVisualStudioHelper -Context $helperContext -Python $nativeTools.imagePython -Source $nativeTools.imageToolDirectory -Destination $destination -Upstream (Join-Path $repositoryRoot '3rdParty\lvgl_image_converter')
    return
}
$imagePython=Join-Path $repositoryRoot 'build\image-tools-env\Scripts\python.exe'
if(!(Test-Path -LiteralPath $imagePython)) { & python -m venv (Join-Path $repositoryRoot 'build\image-tools-env');if($LASTEXITCODE){throw 'Cannot create image conversion environment'} }
& $imagePython -c 'import PIL,png,lz4,resvg_py,PyInstaller' 2>$null
if($LASTEXITCODE) { & $imagePython -m pip install -r (Join-Path $repositoryRoot '3rdParty\lvgl_image_converter\requirements.txt');if($LASTEXITCODE){throw 'Image conversion dependencies failed'} }
$dist=Join-Path $repositoryRoot 'build\image-tool-dist'
& $imagePython -m PyInstaller --noconfirm --onedir --name UICreatorImageTool --paths (Join-Path $repositoryRoot '3rdParty\lvgl_image_converter') --collect-all resvg_py --collect-all lz4 --distpath $dist --workpath (Join-Path $repositoryRoot 'build\image-tool-work') --specpath (Join-Path $repositoryRoot 'build') (Join-Path $PSScriptRoot 'image-converter-bridge.py')
if($LASTEXITCODE){throw 'Image converter packaging failed'}
$destination=Join-Path ([IO.Path]::GetFullPath($OutputDirectory)) 'image-converter'
New-Item -ItemType Directory -Force -Path $destination | Out-Null
Get-ChildItem -LiteralPath (Join-Path $dist 'UICreatorImageTool') -Force | ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination $destination -Recurse -Force}
Copy-Item -LiteralPath (Join-Path $repositoryRoot '3rdParty\lvgl_image_converter') -Destination (Join-Path $destination 'upstream') -Recurse -Force
