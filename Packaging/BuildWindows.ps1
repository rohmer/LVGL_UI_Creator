param(
    [ValidateSet('x64','x86','x32','both')][string]$Architecture='x64',
    [ValidateSet('Release','RelWithDebInfo','Debug')][string]$Configuration='Release',
    [string]$BuildRoot='', [string]$StageRoot='', [string]$X86BuildDirectory='',
    [string]$SdkDirectory='', [int]$Jobs=6, [string]$CMakeExecutable=''
)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $PSScriptRoot 'CMakeTools.ps1')
. (Join-Path $PSScriptRoot 'PeTools.ps1')
$cmakeTools=Get-UICreatorCMakeTools -Executable $CMakeExecutable
if(!$BuildRoot){$BuildRoot=Join-Path $repo 'build\windows-architectures'}
if(!$StageRoot){$StageRoot=Join-Path $repo 'build\windows-installer\payload'}
if(!$SdkDirectory){$SdkDirectory=Join-Path $repo 'build\professional\packages'}
$BuildRoot=[IO.Path]::GetFullPath($BuildRoot);$StageRoot=[IO.Path]::GetFullPath($StageRoot)
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if(!(Test-Path -LiteralPath $vswhere)){throw 'Visual Studio Build Tools with the C++ desktop workload are required.'}
$vsInstall=(& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
if(!$vsInstall){throw 'No installed Visual Studio C++ desktop toolchain was found.'}
$vsCmd=Join-Path $vsInstall 'Common7\Tools\VsDevCmd.bat'
$cmake=$cmakeTools.CMake
$architectures=if($Architecture -eq 'both'){@('x64','x86')}elseif($Architecture -eq 'x32'){@('x86')}else{@($Architecture)}
function BatchPath([string]$Value){return '"'+$Value.Replace('%','%%')+'"'}
foreach($arch in $architectures){
    $buildDir=if($arch -eq 'x86' -and $X86BuildDirectory){[IO.Path]::GetFullPath($X86BuildDirectory)}else{Join-Path $BuildRoot $arch}
    $stage=Join-Path $StageRoot $arch
    New-Item -ItemType Directory -Force -Path $buildDir,(Join-Path $buildDir 'packages') | Out-Null
    if(Test-Path -LiteralPath $SdkDirectory){
        foreach($sdk in @('LVGLProtobuf-ESP32-Toolkit.zip','LVGLProtobuf-Arduino-Toolkit.zip','LVGLProtobuf-Pico-Toolkit.zip','LVGLProtobuf-Linux-Toolkit.zip','LVGLProtobuf-Linux-Keyboard-Profile.zip','LVGLProtobuf-Linux-x86_64.tar.gz','LVGLProtobuf-Linux-aarch64-keyboard.tar.gz')){
            $source=Join-Path $SdkDirectory $sdk;$target=Join-Path (Join-Path $buildDir 'packages') $sdk
            if((Test-Path -LiteralPath $source) -and [IO.Path]::GetFullPath($source) -ne [IO.Path]::GetFullPath($target)){Copy-Item -LiteralPath $source -Destination $target -Force}
        }
    }
    $script=Join-Path $buildDir 'build-native.cmd'
    $lines=@('@echo off','setlocal DisableDelayedExpansion',('call '+(BatchPath $vsCmd)+' -arch='+$arch+' -host_arch=x64 >nul'),'if errorlevel 1 exit /b %errorlevel%',
        ((BatchPath $cmake)+' -S '+(BatchPath $repo)+' -B '+(BatchPath $buildDir)+' -G Ninja -DCMAKE_BUILD_TYPE='+$Configuration+' -DUICREATOR_WINDOWS_ARCH='+$arch+' -DBUILD_TESTING=ON > '+(BatchPath (Join-Path $buildDir 'architecture-configure.log'))+' 2>&1'),
        'if errorlevel 1 exit /b %errorlevel%',
        ((BatchPath $cmake)+' --build '+(BatchPath $buildDir)+' --target UICreator LVGLCompiler UICreatorCodeGen LVGLProtobufWindowsSDK --parallel '+[Math]::Max(1,$Jobs)+' > '+(BatchPath (Join-Path $buildDir 'architecture-build.log'))+' 2>&1'),
        'if errorlevel 1 exit /b %errorlevel%',
        ((BatchPath $cmake)+' --install '+(BatchPath $buildDir)+' --config '+$Configuration+' --prefix '+(BatchPath $stage)+' > '+(BatchPath (Join-Path $buildDir 'architecture-install.log'))+' 2>&1'),
        'exit /b %errorlevel%')
    [IO.File]::WriteAllLines($script,$lines,[Text.Encoding]::ASCII)
    # Stages are generated outputs, not incremental install destinations. Remove
    # stale frozen helpers/CRTs so a repaired x86 build cannot retain x64 files.
    $stage=[IO.Path]::GetFullPath($stage)
    if(!$stage.StartsWith($StageRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($stage) -ne $arch){throw "Unsafe architecture stage directory: $stage"}
    foreach($candidate in @($StageRoot,$stage)){
        if((Test-Path -LiteralPath $candidate) -and ((Get-Item -LiteralPath $candidate -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw "Architecture staging cannot use a reparse point: $candidate"}
    }
    if(Test-Path -LiteralPath $stage){Remove-Item -LiteralPath $stage -Recurse -Force}
    & $env:ComSpec /d /c $script
    if($LASTEXITCODE){throw "The $arch build failed. Inspect architecture-configure.log, architecture-build.log and architecture-install.log in $buildDir"}
    foreach($name in @('UICreator.exe','UICreatorCodeGen.exe','LVGLCompiler.exe','UICreatorViewer.exe','LVGLProtobufDisplay.dll','SDL3.dll')){
        $null=Get-UICreatorPeInfo (Join-Path $stage $name) -Required
    }
    $peAudit=Assert-UICreatorPeArchitecture -Root $stage -Architecture $arch
    $files=@{};Get-ChildItem -LiteralPath $stage -Recurse -File | Where-Object Name -ne 'architecture-manifest.json' | ForEach-Object {
        $relative=$_.FullName.Substring($stage.Length+1).Replace('\','/');$files[$relative]=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    @{version=1;architecture=$arch;configuration=$Configuration;cmakeVersion=$cmakeTools.Version;cmakeExecutable=$cmakeTools.CMake;ctestExecutable=$cmakeTools.CTest;cpackExecutable=$cmakeTools.CPack;peFiles=$peAudit.files;files=$files} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $stage 'architecture-manifest.json') -Encoding UTF8
    Write-Output "Verified $arch application payload: $stage"
}
