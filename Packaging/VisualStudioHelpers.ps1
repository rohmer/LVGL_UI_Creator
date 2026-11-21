# Architecture-aware adapters for the checked-in Visual Studio post-build hooks.
# Reuse the same private x86 builders and clean staging as the CMake release.
. (Join-Path $PSScriptRoot 'PeTools.ps1')
$script:UICreatorVSRepositoryRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))

function Get-UICreatorVisualStudioHelperContext {
    param([Parameter(Mandatory=$true)][string]$OutputDirectory,
          [ValidateSet('auto','x64','x86','Win32','x32')][string]$Architecture='auto')
    $output=[IO.Path]::GetFullPath($OutputDirectory)
    if($Architecture -in @('Win32','x32')){$Architecture='x86'}
    $application=Join-Path $output 'UICreator.exe'
    if(Test-Path -LiteralPath $application -PathType Leaf){
        $information=Get-UICreatorPeInfo $application -Required
        $actual=if($information.machine -eq 0x14c -and $information.bits -eq 32){'x86'}elseif($information.machine -eq 0x8664 -and $information.bits -eq 64){'x64'}else{throw "Unsupported application architecture: $application"}
        if($Architecture -eq 'auto'){$Architecture=$actual}
        elseif($Architecture -ne $actual){throw "Requested $Architecture helpers for a $actual application: $application"}
    }elseif($Architecture -eq 'auto'){throw 'UICreator.exe is required to detect helper architecture. Build the application first or pass -Architecture x64/x86.'}
    return [pscustomobject]@{RepositoryRoot=$script:UICreatorVSRepositoryRoot;OutputDirectory=$output;Architecture=$Architecture}
}

function Get-UICreatorVisualStudioX86Tools {
    param([Parameter(Mandatory=$true)]$Context)
    if($Context.Architecture -ne 'x86'){throw 'Private x86 helpers require an x86 application'}
    $directory=Join-Path $Context.RepositoryRoot 'build\windows-helper-tools\x86'
    $manifest=Join-Path $directory 'cmake-helpers.json'
    $driver=$null
    if(Test-Path -LiteralPath $manifest){
        $previous=Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
        if($previous.imagePython -and (Test-Path -LiteralPath $previous.imagePython -PathType Leaf)){$driver=$previous.imagePython}
    }
    if(!$driver){$driver=(Get-Command python.exe -ErrorAction Stop).Source}
    & $driver -I (Join-Path $Context.RepositoryRoot 'Packaging\BuildWindowsHelpers.py') --architecture x86 --output $directory | Out-Host
    if($LASTEXITCODE){throw 'Native x86 converter preparation failed'}
    $tools=Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
    if($tools.architecture -ne 'x86'){throw 'The helper manifest does not describe x86 tools'}
    foreach($file in @($tools.fontNode,$tools.imagePython,(Join-Path $tools.imageToolDirectory 'UICreatorImageTool.exe'),(Join-Path $tools.projectToolDirectory 'UICreatorProjectTool.exe'))){
        $information=Get-UICreatorPeInfo $file -Required
        if($information.machine -ne 0x14c -or $information.bits -ne 32){throw "Private helper is not a 32-bit executable: $file"}
    }
    return $tools
}

function Reset-UICreatorVisualStudioFontDirectory {
    param([Parameter(Mandatory=$true)]$Context)
    $output=[IO.Path]::GetFullPath($Context.OutputDirectory).TrimEnd([IO.Path]::DirectorySeparatorChar)
    $destination=[IO.Path]::GetFullPath((Join-Path $output 'font-converter'))
    if(!$destination.StartsWith($output+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($destination) -ne 'font-converter'){throw "Unsafe font helper destination: $destination"}
    foreach($candidate in @($output,$destination)){
        if((Test-Path -LiteralPath $candidate) -and ((Get-Item -LiteralPath $candidate -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw "Font helper output cannot use a reparse point: $candidate"}
    }
    if(Test-Path -LiteralPath $destination){
        foreach($entry in Get-ChildItem -LiteralPath $destination -Force -Recurse){if($entry.Attributes -band [IO.FileAttributes]::ReparsePoint){throw "Font helper output contains a reparse point: $($entry.FullName)"}}
        Remove-Item -LiteralPath $destination -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $destination | Out-Null
    return $destination
}

function Sync-UICreatorVisualStudioHelper {
    param([Parameter(Mandatory=$true)]$Context,[Parameter(Mandatory=$true)][string]$Python,
          [Parameter(Mandatory=$true)][string]$Source,[Parameter(Mandatory=$true)][string]$Destination,
          [string]$Upstream='',[string[]]$Keep=@())
    $helperArguments=@('-I',(Join-Path $Context.RepositoryRoot 'Packaging\StageWindowsHelper.py'),$Source,$Destination)
    if($Upstream){$helperArguments+=@('--upstream',$Upstream)}
    foreach($name in $Keep){$helperArguments+=@('--keep',$name)}
    & $Python @helperArguments
    if($LASTEXITCODE){throw "Native helper staging failed: $Destination"}
    $null=Assert-UICreatorPeArchitecture -Root $Destination -Architecture $Context.Architecture -SkipArchives
}
