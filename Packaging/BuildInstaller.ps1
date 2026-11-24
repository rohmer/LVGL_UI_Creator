param(
    [string]$StageRoot='', [string]$BuildDirectory='', [string]$OutputDirectory='',
    [ValidatePattern('^[0-9]+\.[0-9]+\.[0-9]+$')][string]$Version='0.9.2',
    [ValidateSet('both','x64','x86','combined')][string]$Architecture='both',
    [switch]$TestInstaller, [switch]$SmokeApplications, [string]$CMakeExecutable='', [string]$NsisExecutable='', [string]$PythonExecutable=''
)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $PSScriptRoot 'CMakeTools.ps1')
. (Join-Path $PSScriptRoot 'PeTools.ps1')
$cmakeTools=Get-UICreatorCMakeTools -Executable $CMakeExecutable
$nsisTool=$null
if($NsisExecutable){$nsisTool=Get-UICreatorNsisTool -Executable $NsisExecutable}
if(!$StageRoot){$StageRoot=Join-Path $repo 'build\windows-installer\payload'}
if(!$BuildDirectory){$BuildDirectory=Join-Path $repo 'build\windows-installer\cpack'}
if(!$OutputDirectory){$OutputDirectory=Join-Path $repo 'build\windows-installer\packages'}
$StageRoot=[IO.Path]::GetFullPath($StageRoot)
$BuildDirectory=[IO.Path]::GetFullPath($BuildDirectory)
$OutputDirectory=[IO.Path]::GetFullPath($OutputDirectory)
$cmake=$cmakeTools.CMake
$cpack=$cmakeTools.CPack

$selectedArchitectures=if($Architecture -in @('both','combined')){@('x64','x86')}else{@($Architecture)}
foreach($validatedArchitecture in $selectedArchitectures){
    $stage=Join-Path $StageRoot $validatedArchitecture
    $manifestPath=Join-Path $stage 'architecture-manifest.json'
    if(!(Test-Path -LiteralPath $manifestPath -PathType Leaf)){throw "Missing $validatedArchitecture payload. Run Packaging/BuildWindows.ps1 -Architecture both first: $manifestPath"}
    $manifest=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if($manifest.version -ne 1 -or $manifest.architecture -ne $validatedArchitecture){throw "Invalid architecture manifest: $manifestPath"}
    $entries=@($manifest.files.PSObject.Properties)
    if(!$entries.Count){throw "Empty architecture manifest: $manifestPath"}
    $known=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($entry in $entries){
        $relative=$entry.Name
        if($relative -match '[\x00-\x1f;"$]' -or $relative.Contains('\') -or [IO.Path]::IsPathRooted($relative)) {throw "Unsafe manifest path: $relative"}
        $file=[IO.Path]::GetFullPath((Join-Path $stage $relative))
        if(!$file.StartsWith($stage+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw "Manifest path escapes payload: $relative"}
        if(!$known.Add($file)){throw "Duplicate manifest path: $relative"}
        if(!(Test-Path -LiteralPath $file -PathType Leaf)){throw "Missing payload file: $validatedArchitecture/$relative"}
        if($entry.Value -notmatch '^[0-9a-fA-F]{64}$' -or (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $entry.Value){throw "Payload hash mismatch: $validatedArchitecture/$relative"}
    }
    foreach($file in Get-ChildItem -LiteralPath $stage -Recurse -File){
        if($file.Name -eq 'architecture-manifest.json' -and $file.DirectoryName -eq $stage){continue}
        if(!$known.Contains($file.FullName)){throw "Unmanifested payload file: $($file.FullName)"}
    }
    foreach($name in @('UICreator.exe','UICreatorCodeGen.exe','LVGLCompiler.exe','UICreatorViewer.exe','LVGLProtobufDisplay.dll','SDL3.dll')){
        $null=Get-UICreatorPeInfo (Join-Path $stage $name) -Required
    }
    $peAudit=Assert-UICreatorPeArchitecture -Root $stage -Architecture $validatedArchitecture
    Write-Output "Verified $validatedArchitecture payload: $($entries.Count) files, $($peAudit.files.Count) native PE files including SDK archives."
}
New-Item -ItemType Directory -Force -Path $BuildDirectory,$OutputDirectory | Out-Null
$plans=if($Architecture -eq 'combined'){@('combined')}else{$selectedArchitectures}
$packageResults=[Collections.Generic.List[object]]::new()
foreach($packageArchitecture in $plans){
    $combined=$packageArchitecture -eq 'combined'
    $packageBuild=if($combined){$BuildDirectory}else{Join-Path $BuildDirectory $packageArchitecture}
    $packageSource=if($combined){Join-Path $PSScriptRoot 'Combined'}else{Join-Path $PSScriptRoot 'Single'}
    $configureArguments=@('-S',$packageSource,'-B',$packageBuild,'-G','Ninja',"-DSTAGE_ROOT=$($StageRoot.Replace('\','/'))","-DRELEASE_VERSION=$Version")
    if(!$combined){$configureArguments+="-DAPPLICATION_ARCHITECTURE=$packageArchitecture"}
    if($nsisTool){$configureArguments+="-DCPACK_NSIS_EXECUTABLE:FILEPATH=$($nsisTool.Executable.Replace('\','/'))"}
    & $cmake @configureArguments
    if($LASTEXITCODE){throw "$packageArchitecture installer configuration failed."}
    & $cpack --config (Join-Path $packageBuild 'CPackConfig.cmake') -G NSIS -B $OutputDirectory
    if($LASTEXITCODE){throw "CPack NSIS $packageArchitecture packaging failed. Inspect $OutputDirectory/_CPack_Packages"}
    $stem=if($combined){"LVGL-UI-Creator-$Version-windows-setup"}else{"LVGL-UI-Creator-$Version-windows-$packageArchitecture-setup"}
    $installer=Join-Path $OutputDirectory ($stem+'.exe')
    $hash=(Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash.ToLowerInvariant()
    [IO.File]::WriteAllText($installer+'.sha256',"$hash  $([IO.Path]::GetFileName($installer))`n",[Text.Encoding]::ASCII)
    $payloadHashes=@{}
    foreach($payloadArchitecture in $(if($combined){@('x64','x86')}else{@($packageArchitecture)})){
        $payloadHashes[$payloadArchitecture]=(Get-FileHash -LiteralPath (Join-Path $StageRoot "$payloadArchitecture/architecture-manifest.json") -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    $package=@{
        version=1;release=$Version;generator='CPack NSIS';architecture=$packageArchitecture
        cmakeVersion=$cmakeTools.Version;cmakeExecutable=$cmakeTools.CMake;ctestExecutable=$cmakeTools.CTest;cpackExecutable=$cmakeTools.CPack
        nsisExecutable=if($nsisTool){$nsisTool.Executable}else{''};nsisVersion=if($nsisTool){$nsisTool.Version}else{''}
        installer=@{file=[IO.Path]::GetFileName($installer);sha256=$hash;bytes=(Get-Item -LiteralPath $installer).Length}
        payloadManifests=$payloadHashes
    }
    $package | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $OutputDirectory ($stem+'.manifest.json')) -Encoding UTF8
    $packageResults.Add($package)
    Write-Output "Windows $packageArchitecture installer: $installer"
    if($TestInstaller -or $SmokeApplications){& (Join-Path $PSScriptRoot 'TestInstaller.ps1') -Installer $installer -StageRoot $StageRoot -Architecture $packageArchitecture -SmokeApplications:$SmokeApplications -PythonExecutable $PythonExecutable}
}
# Rebuilding one architecture must preserve the verified release entry for the
# other setup, just as it preserves the other setup executable itself.
$catalogPath=Join-Path $OutputDirectory 'installer.manifest.json'
if($Architecture -in @('x64','x86') -and (Test-Path -LiteralPath $catalogPath)){
    $previous=Get-Content -LiteralPath $catalogPath -Raw | ConvertFrom-Json
    foreach($existing in $previous.packages){
        if($existing.release -ne $Version -or $existing.architecture -notin @('x64','x86') -or $existing.architecture -eq $Architecture){continue}
        $existingPath=Join-Path $OutputDirectory $existing.installer.file
        if((Test-Path -LiteralPath $existingPath -PathType Leaf) -and (Get-FileHash -LiteralPath $existingPath -Algorithm SHA256).Hash -eq $existing.installer.sha256){$packageResults.Add($existing)}
    }
}
$catalogArchitectures=@($packageResults | ForEach-Object {if($_.architecture -eq 'combined'){@('x64','x86')}else{$_.architecture}} | Sort-Object -Unique)
@{version=1;release=$Version;generator='CPack NSIS';architectures=$catalogArchitectures;packages=$packageResults.ToArray()} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $catalogPath -Encoding UTF8
