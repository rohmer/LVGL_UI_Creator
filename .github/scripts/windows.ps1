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
    $keyboardRoot=Join-Path $repo '3rdParty/AdvancedKeyboard'
    $nativeHelper=Join-Path $repo 'build/ci-native-helper/UICreatorModelGen.exe'
    cmake -S "$keyboardRoot/tools/NativeTrieGenerator" -B build/ci-trie -G Ninja -DCMAKE_BUILD_TYPE=Release "-DADVKBD_NATIVE_HELPER_OUTPUT=$repo/build/ci-native-helper"
    if($LASTEXITCODE){exit $LASTEXITCODE}
    cmake --build build/ci-trie --parallel 2
    if($LASTEXITCODE){exit $LASTEXITCODE}
    cmake -S Tests/Integration/RadixPrediction -B build/ci-radix -G Ninja -DCMAKE_BUILD_TYPE=Release
    if($LASTEXITCODE){exit $LASTEXITCODE}
    cmake --build build/ci-radix --target radix_prediction --parallel 2
    if($LASTEXITCODE){exit $LASTEXITCODE}
    dotnet build "$keyboardRoot/tools/CorpusWorkbench/CorpusWorkbench.csproj" -c Release
    if($LASTEXITCODE){exit $LASTEXITCODE}
    dotnet run --project "$keyboardRoot/tools/CorpusLanguageTests/CorpusLanguageTests.csproj" -c Release -- $keyboardRoot $nativeHelper
    if($LASTEXITCODE){exit $LASTEXITCODE}
    dotnet run --project "$keyboardRoot/tools/KeyboardSimulator.Tests/KeyboardSimulator.LanguageTests.csproj" -c Release -- (Join-Path $keyboardRoot 'tools/layout') --native $nativeHelper --policy-probe (Join-Path $repo 'build/ci-radix/radix_prediction.exe')
    if($LASTEXITCODE){exit $LASTEXITCODE}
    $parityBuild=Join-Path $repo 'build/ci-keyboard-parity'
    $parityAssembly=Join-Path $keyboardRoot 'build/corpus-tools/bin/KeyboardParity/Release/net8.0-windows/KeyboardParity.dll'
    dotnet build "$keyboardRoot/tools/KeyboardParity/KeyboardParity.csproj" -c Release
    if($LASTEXITCODE){exit $LASTEXITCODE}
    cmake -S "$keyboardRoot/tools/KeyboardParity" -B $parityBuild -G Ninja -DCMAKE_BUILD_TYPE=Release "-DADVKBD_PARITY_ASSEMBLY=$parityAssembly"
    if($LASTEXITCODE){exit $LASTEXITCODE}
    cmake --build $parityBuild --parallel 2
    if($LASTEXITCODE){exit $LASTEXITCODE}
    ctest --test-dir $parityBuild --output-on-failure --no-tests=error
    exit $LASTEXITCODE
}
python .github/scripts/desktop.py --architecture $Architecture --configuration $Configuration --phase $Phase --jobs 2
exit $LASTEXITCODE
