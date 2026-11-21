param([Parameter(Mandatory=$true)][string]$CMakeExecutable,[string]$ExpectedVersion='',[string]$NsisExecutable='')
$ErrorActionPreference='Stop'
$repository=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
. (Join-Path $repository 'Packaging/CMakeTools.ps1')
function Require([bool]$Condition,[string]$Message){if(!$Condition){throw $Message}}
function Reject([scriptblock]$Action,[string]$Expected){
    try{& $Action | Out-Null}catch{if($_.Exception.Message.Contains($Expected)){return};throw}
    throw "Expected a rejection containing: $Expected"
}
$tools=Get-UICreatorCMakeTools -Executable $CMakeExecutable
Require ([IO.Path]::IsPathRooted($tools.CMake)) 'CMake path was not resolved absolutely'
if($ExpectedVersion){Require ($tools.Version -eq $ExpectedVersion) "Expected CMake $ExpectedVersion, selected $($tools.Version)"}
Require ((Split-Path $tools.CTest -Parent) -eq (Split-Path $tools.CMake -Parent)) 'CTest came from a different installation'
Require ((Split-Path $tools.CPack -Parent) -eq (Split-Path $tools.CMake -Parent)) 'CPack came from a different installation'
# A process-local PATH selection exercises the no-argument compatibility path.
$originalTaskPath=$env:PATH
try{
    $env:PATH=(Split-Path $tools.CMake -Parent)+[IO.Path]::PathSeparator+$originalTaskPath
    $defaultTools=Get-UICreatorCMakeTools
    Require ($defaultTools.CMake -eq $tools.CMake) 'Default selection did not honor the process-local toolset'
}finally{$env:PATH=$originalTaskPath}
Reject {Get-UICreatorCMakeTools -Executable (Join-Path $repository 'build/no-such-cmake-tool.exe')} 'not found'
Reject {Get-UICreatorCMakeTools -Executable $tools.CTest} 'Cannot identify cmake version'
$fixture=Join-Path $repository ('build/cmake-toolset-tests/'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
$old=Join-Path $fixture 'old-cmake.cmd'
[IO.File]::WriteAllLines($old,@('@echo off','echo cmake version 3.23.0','exit /b 0'),[Text.Encoding]::ASCII)
Reject {Get-UICreatorCMakeTools -Executable $old} 'CMake 3.24 or newer is required'
$incomplete=Join-Path $fixture 'cmake.cmd'
[IO.File]::WriteAllLines($incomplete,@('@echo off',('echo cmake version '+$tools.Version),'exit /b 0'),[Text.Encoding]::ASCII)
Reject {Get-UICreatorCMakeTools -Executable $incomplete} 'toolset is incomplete'
if($NsisExecutable){
    $nsis=Get-UICreatorNsisTool -Executable $NsisExecutable
    Require ([IO.Path]::IsPathRooted($nsis.Executable)) 'NSIS compiler path was not resolved absolutely'
    Require (([version]$nsis.Version) -ge [version]'3.3') 'NSIS compiler is older than CPack supports'
    Reject {Get-UICreatorNsisTool -Executable (Join-Path $repository 'build/no-such-nsis.exe')} 'NSIS compiler was not found'
}
Write-Output "CMake $($tools.Version) absolute toolset selection, sibling CTest/CPack, PATH fallback, old-version rejection and incomplete-toolset rejection passed"
