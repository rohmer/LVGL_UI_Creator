param(
    [ValidateSet('x64','x86','Win32')][string]$Architecture='x64',
    [ValidateSet('Debug','Release')][string]$Configuration='Release',
    [string]$VisualStudioRoot='', [string]$ToolsVersion='',
    [string]$CMakeExecutable='', [int]$Jobs=2, [switch]$Verify
)
$ErrorActionPreference='Stop'
if($Architecture -eq 'Win32'){$Architecture='x86'}
if($Jobs -lt 1){throw 'Jobs must be positive'}
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $PSScriptRoot 'CMakeTools.ps1')
$cmakeTools=Get-UICreatorCMakeTools -Executable $CMakeExecutable
if(!$VisualStudioRoot){
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $VisualStudioRoot=(& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
}
if(!$VisualStudioRoot){throw 'Visual Studio C++ desktop tools are required'}
$VisualStudioRoot=[IO.Path]::GetFullPath($VisualStudioRoot)
Import-Module (Join-Path $VisualStudioRoot 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
$devArguments="-arch=$Architecture -host_arch=x64"
if($ToolsVersion){$devArguments+=" -vcvars_ver=$ToolsVersion"}
Enter-VsDevShell -VsInstallPath $VisualStudioRoot -SkipAutomaticLocation -DevCmdArguments $devArguments
if(!$env:CARGO_BUILD_JOBS){$env:CARGO_BUILD_JOBS=[string]$Jobs}
$build=Join-Path $repo "build/visual-studio-native-converters/$Architecture"
$source=Join-Path $PSScriptRoot 'VisualStudioConverters'
$verifyValue=if($Verify){'ON'}else{'OFF'}
& $cmakeTools.CMake -S $source -B $build -G 'Ninja Multi-Config' "-DUICREATOR_VS_VERIFY_CONVERTER_LIBRARIES=$verifyValue"
if($LASTEXITCODE){throw "Native converter dependency configuration failed for $Architecture"}
& $cmakeTools.CMake --build $build --config $Configuration --target UICreatorVSNativeConverterLibraries --parallel $Jobs
if($LASTEXITCODE){throw "Native converter dependency build failed for $Architecture $Configuration"}
if($Verify){
    & $cmakeTools.CTest --test-dir $build -C $Configuration --output-on-failure
    if($LASTEXITCODE){throw "Native converter dependency checks failed for $Architecture $Configuration"}
}
$libraryDirectory=Join-Path $build "$Configuration/lib"
foreach($name in @('UICreatorNativePng','UICreatorNativeFreeType','UICreatorBrotliDecoder','UICreatorBrotliCommon','UICreatorImageQuant','UICreatorResvg')){
    if(!(Test-Path -LiteralPath (Join-Path $libraryDirectory "$name.lib") -PathType Leaf)){throw "Missing native converter library: $name"}
}
Write-Host "Native converter libraries ready: $Architecture $Configuration ($libraryDirectory)"
