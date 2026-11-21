param([Parameter(Mandatory=$true)][string]$OutputDirectory,[ValidateSet('auto','x64','x86','Win32','x32')][string]$Architecture='auto')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\..\Packaging\VisualStudioHelpers.ps1')
$helperContext=Get-UICreatorVisualStudioHelperContext -OutputDirectory $OutputDirectory -Architecture $Architecture
$repositoryRoot=$helperContext.RepositoryRoot
$converterSource=Join-Path $repositoryRoot '3rdParty\lv_font_conv'
$nativeTools=$null
if($helperContext.Architecture -eq 'x86'){$nativeTools=Get-UICreatorVisualStudioX86Tools $helperContext;$bundledNode=$nativeTools.fontNode}
else{$bundledNode=Join-Path $repositoryRoot 'build\font-tools\node-v24.11.1-win-x64\node.exe'}
if(!(Test-Path -LiteralPath $bundledNode)) { $bundledNode=(Get-Command node.exe -ErrorAction Stop).Source }
if(!(Test-Path -LiteralPath (Join-Path $converterSource 'node_modules\argparse\package.json'))) {
    Push-Location $converterSource
    $fontNpm=if($nativeTools){$nativeTools.fontNpm}else{'npm.cmd'}
    try { & $fontNpm ci --omit=dev --ignore-scripts --no-audit --no-fund; if($LASTEXITCODE){throw 'Font converter dependency installation failed'} }
    finally { Pop-Location }
}
$fontToolDestination=if($nativeTools){Reset-UICreatorVisualStudioFontDirectory $helperContext}else{Join-Path ([IO.Path]::GetFullPath($OutputDirectory)) 'font-converter'}
New-Item -ItemType Directory -Force -Path $fontToolDestination | Out-Null
Get-ChildItem -LiteralPath $converterSource -Force | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $fontToolDestination -Recurse -Force }
Copy-Item -LiteralPath $bundledNode -Destination (Join-Path $fontToolDestination 'node.exe') -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'font-converter-bridge.js') -Destination (Join-Path $fontToolDestination 'bridge.js') -Force
$nodeLicense=Join-Path (Split-Path -Parent $bundledNode) 'LICENSE'
if(Test-Path -LiteralPath $nodeLicense) {Copy-Item -LiteralPath $nodeLicense -Destination (Join-Path $fontToolDestination 'NODE_LICENSE') -Force}
if($nativeTools){$null=Assert-UICreatorPeArchitecture -Root $fontToolDestination -Architecture x86 -SkipArchives}
