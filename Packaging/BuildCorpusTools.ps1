param(
    [ValidateSet('x64','x86','both')][string]$Architecture='both',
    [string]$OutputDirectory='', [string]$PublishRoot='', [string]$NativePayloadRoot='',
    [string]$CMakeExecutable=''
)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $PSScriptRoot 'CMakeTools.ps1')
. (Join-Path $PSScriptRoot 'PeTools.ps1')
$cmakeTools=Get-UICreatorCMakeTools -Executable $CMakeExecutable
if(!$OutputDirectory){$OutputDirectory=Join-Path $repo 'build/corpus-tools-packages'}
if(!$PublishRoot){$PublishRoot=Join-Path $repo 'build/corpus-tools-publish'}
if(!$NativePayloadRoot){$NativePayloadRoot=Join-Path $repo 'build/windows-installer/payload'}
$OutputDirectory=[IO.Path]::GetFullPath($OutputDirectory)
$PublishRoot=[IO.Path]::GetFullPath($PublishRoot)
$lock=Get-Content -LiteralPath (Join-Path $repo 'advanced-keyboard.lock.json') -Raw | ConvertFrom-Json
$source=Join-Path $repo $lock.paths.creator
$revision=(& git -C $source rev-parse HEAD)
if($LASTEXITCODE -or $revision -ne $lock.revision){throw 'AdvancedKeyboard must match the pinned revision before packaging.'}
$sourceStatus=(& git -C $source status --porcelain --untracked-files=all)
if($LASTEXITCODE -or $sourceStatus){throw 'Publish shared tool changes before creating release packages.'}
$architectures=if($Architecture -eq 'both'){@('x64','x86')}else{@($Architecture)}
$version=([regex]::Match((Get-Content (Join-Path $repo 'CMakeLists.txt') -Raw), 'project\([^)]*VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)')).Groups[1].Value
if(!$version){throw 'Cannot read the release version from CMakeLists.txt.'}
New-Item -ItemType Directory -Force -Path $OutputDirectory,$PublishRoot | Out-Null
$nugetCache=((& dotnet nuget locals global-packages --list) -replace '^global-packages:\s*','').Trim()
if($LASTEXITCODE -or !$nugetCache){throw 'Cannot locate the NuGet package cache for runtime notices.'}
foreach($arch in $architectures){
    $payload=Join-Path $PublishRoot $arch
    # Refuse reused output folders so a changed runtime cannot retain stale files.
    if(Test-Path -LiteralPath $payload){throw "Choose an unused publish directory: $payload"}
    $native=Join-Path $NativePayloadRoot $arch
    $helper=Join-Path $native 'UICreatorModelGen.exe'
    if(!(Test-Path -LiteralPath $helper -PathType Leaf)){throw "Build the matching Windows payload first: $helper"}
    $wordlists=Join-Path $native 'language-models/wordlists'
    foreach($wordlist in @('words_alpha.txt','words_de_top150000.txt','words_el_top150000.txt','words_es_top150000.txt','words_fr_top150000.txt')){
        if(!(Test-Path -LiteralPath (Join-Path $wordlists $wordlist) -PathType Leaf)){throw "Native model generator word list is missing: $wordlist"}
    }
    & dotnet publish (Join-Path $source 'tools/CorpusWorkbench/CorpusWorkbench.csproj') -c Release -r "win-$arch" --self-contained true --nologo -m:1 -nodeReuse:false -o $payload -p:PublishSingleFile=false -p:PublishTrimmed=false -p:DebugType=None -p:DebugSymbols=false
    if($LASTEXITCODE){throw "Corpus Workbench publish failed for $arch."}
    Copy-Item -LiteralPath $helper -Destination $payload
    $languageModels=Join-Path $payload 'language-models'
    New-Item -ItemType Directory -Force -Path $languageModels | Out-Null
    Copy-Item -LiteralPath $wordlists -Destination $languageModels -Recurse
    Get-ChildItem -LiteralPath $native -File | Where-Object Name -Match '^(msvcp|vcruntime|concrt|vccorlib).*\.dll$' | Copy-Item -Destination $payload
    # The generated copy must not distribute a build host's personal Java path.
    $javaHome=Join-Path $payload 'OpenNlp/java-home.txt'
    if(Test-Path -LiteralPath $javaHome -PathType Leaf){Remove-Item -LiteralPath $javaHome}
    $licenses=Join-Path $payload 'licenses'
    New-Item -ItemType Directory -Force -Path $licenses | Out-Null
    foreach($notice in @('LICENSE','NOTICE.md','tools/CorpusWorkbench/README.md','tools/KeyboardSimulator/README.md')){
        $name=if($notice -eq 'tools/CorpusWorkbench/README.md'){'CorpusWorkbench.md'}elseif($notice -eq 'tools/KeyboardSimulator/README.md'){'KeyboardSimulator.md'}else{[IO.Path]::GetFileName($notice)}
        Copy-Item -LiteralPath (Join-Path $source $notice) -Destination (Join-Path $licenses $name)
    }
    Copy-Item -LiteralPath (Join-Path $repo 'advanced-keyboard.lock.json') -Destination $payload
    $trainerNotices=Join-Path $licenses 'native-trainer'
    New-Item -ItemType Directory -Force -Path $trainerNotices | Out-Null
    Get-ChildItem -LiteralPath (Join-Path $source 'tools/NativeTrieGenerator/source/wordlists') -File | Where-Object Name -Match 'LICENSE|NOTICE' | Copy-Item -Destination $trainerNotices
    $expected=if($arch -eq 'x64'){0x8664}else{0x14c}
    # App hosts and the actual CLR/JIT are native; managed AnyCPU assemblies
    # intentionally do not have to match their host's PE machine.
    $nativeFiles=@('CorpusWorkbench.exe','KeyboardSimulator.exe','UICreatorModelGen.exe','coreclr.dll','clrjit.dll','hostfxr.dll','hostpolicy.dll')
    foreach($name in $nativeFiles){
        $information=Get-UICreatorPeInfo (Join-Path $payload $name) -Required
        if($information.machine -ne $expected){throw "Wrong $arch native tool/runtime: $name"}
    }
    foreach($name in @('CorpusWorkbench','KeyboardSimulator')){
        $runtime=Get-Content -LiteralPath (Join-Path $payload "$name.runtimeconfig.json") -Raw | ConvertFrom-Json
        if($runtime.runtimeOptions.framework -or $runtime.runtimeOptions.frameworks){throw "The $name runtime is not self-contained."}
    }
    foreach($framework in $runtime.runtimeOptions.includedFrameworks){
        $packageName=if($framework.name -eq 'Microsoft.NETCore.App'){"microsoft.netcore.app.runtime.win-$arch"}elseif($framework.name -eq 'Microsoft.WindowsDesktop.App'){"microsoft.windowsdesktop.app.runtime.win-$arch"}else{throw "Unexpected bundled runtime: $($framework.name)"}
        $runtimePackage=Join-Path $nugetCache ($packageName+'/'+$framework.version)
        $notices=@(Get-ChildItem -LiteralPath $runtimePackage -File | Where-Object Name -Match 'LICENSE|NOTICE')
        if(!$notices.Count){throw "Bundled runtime license notices are missing: $runtimePackage"}
        $destination=Join-Path $licenses $framework.name
        New-Item -ItemType Directory -Force -Path $destination | Out-Null
        $notices | Copy-Item -Destination $destination
    }
    $assets=Get-Content -LiteralPath (Join-Path $source 'build/corpus-tools/obj/CorpusWorkbench/project.assets.json') -Raw | ConvertFrom-Json
    $packageNotices=[ordered]@{}
    foreach($library in $assets.libraries.PSObject.Properties | Where-Object {$_.Value.type -eq 'package'}){
        $package=Join-Path $nugetCache $library.Value.path
        $destination=Join-Path (Join-Path $licenses 'nuget') $library.Value.path
        $copied=@()
        foreach($notice in Get-ChildItem -LiteralPath $package -File -Recurse | Where-Object {$_.Name -match 'LICENSE|NOTICE|COPYING|\.nuspec$'}){
            $relative=$notice.FullName.Substring($package.Length+1)
            $target=Join-Path $destination $relative
            New-Item -ItemType Directory -Force -Path ([IO.Path]::GetDirectoryName($target)) | Out-Null
            Copy-Item -LiteralPath $notice.FullName -Destination $target
            $copied+=$relative.Replace('\','/')
        }
        $packageNotices[$library.Name]=$copied
    }
    $packageNotices | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $licenses 'nuget-license-inventory.json') -Encoding UTF8
    if(!(Test-Path -LiteralPath (Join-Path $payload 'Keyboards/en-US-qwerty.json'))){throw 'Keyboard layouts are missing.'}
    $files=[ordered]@{}
    Get-ChildItem -LiteralPath $payload -File -Recurse | Sort-Object FullName | ForEach-Object {
        $relative=$_.FullName.Substring($payload.Length+1).Replace('\','/')
        $files[$relative]=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    @{version=1;architecture=$arch;keyboardRevision=$lock.revision;selfContained=$true;nativeFiles=$nativeFiles;files=$files} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $payload 'corpus-tools-manifest.json') -Encoding UTF8
    $packageBuild=Join-Path $PublishRoot "cpack-$arch"
    & $cmakeTools.CMake -S (Join-Path $PSScriptRoot 'CorpusTools') -B $packageBuild -G Ninja "-DTOOLS_PAYLOAD=$($payload.Replace('\','/'))" "-DTOOLS_ARCHITECTURE=$arch" "-DRELEASE_VERSION=$version"
    if($LASTEXITCODE){throw "Corpus tools packaging configuration failed for $arch."}
    & $cmakeTools.CPack --config (Join-Path $packageBuild 'CPackConfig.cmake') -G ZIP -B $OutputDirectory
    if($LASTEXITCODE){throw "Corpus tools packaging failed for $arch."}
    $archive=Join-Path $OutputDirectory "AdvancedKeyboard-CorpusTools-$version-windows-$arch.zip"
    $hash=(Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
    [IO.File]::WriteAllText($archive+'.sha256', $hash+'  '+[IO.Path]::GetFileName($archive)+"`n", [Text.Encoding]::ASCII)
    Write-Output "Packaged self-contained $arch Corpus Workbench and Keyboard Simulator: $archive"
}
