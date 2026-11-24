param(
    [ValidateSet('x86','x64')][string]$Architecture='x64',
    [ValidateSet('Debug','Release')][string]$Configuration='Release',
    [ValidateSet('configure','build','test','all')][string]$Phase='all',
    [switch]$Managed
)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'No Visual Studio C++ desktop compiler found'}
Import-Module (Join-Path $vs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments "-arch=$Architecture -host_arch=x64"
if($env:UICREATOR_CI_CMAKE_BIN){$env:PATH=$env:UICREATOR_CI_CMAKE_BIN+[IO.Path]::PathSeparator+$env:PATH}
Set-Location -LiteralPath $repo
if($Managed){
    cmake -S Tools/NativeTrieGenerator -B build/ci-trie -G Ninja -DCMAKE_BUILD_TYPE=Release
    if($LASTEXITCODE){exit $LASTEXITCODE}
    cmake --build build/ci-trie --parallel 2
    if($LASTEXITCODE){exit $LASTEXITCODE}
    dotnet build Tools/CorpusWorkbench/CorpusWorkbench.csproj -c Release
    if($LASTEXITCODE){exit $LASTEXITCODE}
    dotnet run --project Tools/CorpusLanguageTests/CorpusLanguageTests.csproj -c Release -- $repo (Join-Path $repo 'Tools/NativeTrieGenerator/bin/UICreatorModelGen.exe')
    if($LASTEXITCODE){exit $LASTEXITCODE}
    dotnet run --project Tools/KeyboardSimulator.Tests/KeyboardSimulator.LanguageTests.csproj -c Release -- (Join-Path $repo 'LVSerial/Widgets/AdvancedKeyboard/layout')
    exit $LASTEXITCODE
}
python .github/scripts/desktop.py --architecture $Architecture --configuration $Configuration --phase $Phase --jobs 2
exit $LASTEXITCODE
