param([string]$WorkDirectory='')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
. (Join-Path $repo 'Packaging/PeTools.ps1')
if(!$WorkDirectory){$WorkDirectory=Join-Path $repo ('build/pe-architecture-test-'+[Guid]::NewGuid().ToString('N'))}
$WorkDirectory=[IO.Path]::GetFullPath($WorkDirectory)
if(Test-Path -LiteralPath $WorkDirectory){throw 'Use a fresh PE fixture directory.'}
New-Item -ItemType Directory -Path $WorkDirectory | Out-Null
function Write-Pe([string]$Path,[int]$Machine=0x14c,[int]$Magic=0x10b){
    New-Item -ItemType Directory -Force -Path ([IO.Path]::GetDirectoryName($Path)) | Out-Null
    $bytes=New-Object byte[] 512
    [BitConverter]::GetBytes([uint16]0x5a4d).CopyTo($bytes,0)
    [BitConverter]::GetBytes([uint32]128).CopyTo($bytes,0x3c)
    [BitConverter]::GetBytes([uint32]0x4550).CopyTo($bytes,128)
    [BitConverter]::GetBytes([uint16]$Machine).CopyTo($bytes,132)
    [BitConverter]::GetBytes([uint16]224).CopyTo($bytes,148)
    [BitConverter]::GetBytes([uint16]$Magic).CopyTo($bytes,152)
    [IO.File]::WriteAllBytes($Path,$bytes)
}
function Reject([scriptblock]$Action,[string]$Expected){
    try{& $Action | Out-Null}catch{if($_.Exception.Message -notmatch $Expected){throw};return}
    throw "Invalid PE fixture was accepted: $Expected"
}
$valid=Join-Path $WorkDirectory 'valid';Write-Pe (Join-Path $valid 'UICreator.exe');Write-Pe (Join-Path $valid 'worker/_internal/module.pyd');Write-Pe (Join-Path $valid 'font/module.node')
[IO.File]::WriteAllText((Join-Path $valid 'data.txt'),'ordinary non-native asset')
$result=Assert-UICreatorPeArchitecture -Root $valid -Architecture x86
if($result.files.Count -ne 3){throw 'Recursive native-module inventory is incomplete.'}
Reject {Assert-UICreatorPeArchitecture -Root $valid -Architecture x64} 'Architecture mismatch'
$nested=Join-Path $WorkDirectory 'nested';Write-Pe (Join-Path $nested 'worker/_internal/vcruntime.dll') 0x8664 0x20b
Reject {Assert-UICreatorPeArchitecture -Root $nested -Architecture x86} 'worker/_internal/vcruntime.dll'
$extensionless=Join-Path $WorkDirectory 'extensionless';Write-Pe (Join-Path $extensionless 'worker/native-payload') 0x8664 0x20b
Reject {Assert-UICreatorPeArchitecture -Root $extensionless -Architecture x86} 'Architecture mismatch'
$wrongMagic=Join-Path $WorkDirectory 'wrong-magic';Write-Pe (Join-Path $wrongMagic 'bad.exe') 0x14c 0x20b
Reject {Assert-UICreatorPeArchitecture -Root $wrongMagic -Architecture x86} '64-bit image'
$malformed=Join-Path $WorkDirectory 'malformed';New-Item -ItemType Directory -Path $malformed | Out-Null
[IO.File]::WriteAllText((Join-Path $malformed 'bad.pyd'),'not a PE image')
Reject {Assert-UICreatorPeArchitecture -Root $malformed -Architecture x86} 'Not a Windows PE'
$truncated=Join-Path $WorkDirectory 'truncated';New-Item -ItemType Directory -Path $truncated | Out-Null
[IO.File]::WriteAllBytes((Join-Path $truncated 'bad.exe'),[byte[]](0x4d,0x5a))
Reject {Assert-UICreatorPeArchitecture -Root $truncated -Architecture x86} 'Truncated DOS'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$sdk=Join-Path $WorkDirectory 'sdk';Write-Pe (Join-Path $sdk 'app.exe')
$archiveSource=Join-Path $WorkDirectory 'sdk-source';Write-Pe (Join-Path $archiveSource 'bin/runtime.dll')
[IO.Compression.ZipFile]::CreateFromDirectory($archiveSource,(Join-Path $sdk 'device.zip'))
$result=Assert-UICreatorPeArchitecture -Root $sdk -Architecture x86
if($result.files.Count -ne 2 -or !($result.files.path -contains 'device.zip!/bin/runtime.dll')){throw 'SDK ZIP native inventory is incomplete.'}
$foreignSdk=Join-Path $WorkDirectory 'foreign-sdk';Write-Pe (Join-Path $foreignSdk 'python/module.pyd') 0x8664 0x20b
[IO.Compression.ZipFile]::CreateFromDirectory($foreignSdk,(Join-Path $sdk 'foreign.zip'))
Reject {Assert-UICreatorPeArchitecture -Root $sdk -Architecture x86} 'foreign.zip!/python/module.pyd'
Write-Output 'PASS: recursive PE architecture, native extensions, extensionless images, PE32 format, malformed input and SDK ZIP validation.'
