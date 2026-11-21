# Resolve a complete CMake toolset without changing the machine's PATH.
function Resolve-UICreatorApplicationPath {
    param([Parameter(Mandatory=$true)][string]$Command)
    if([IO.File]::Exists($Command)){return [IO.Path]::GetFullPath($Command)}
    if([IO.Path]::IsPathRooted($Command) -or $Command.Contains('\') -or $Command.Contains('/')){throw "Executable does not exist: $Command"}
    $names=@($Command)
    if(![IO.Path]::GetExtension($Command)){
        $extensions=[Environment]::GetEnvironmentVariable('PATHEXT','Process')
        if(!$extensions){$extensions='.COM;.EXE;.BAT;.CMD'}
        foreach($extension in $extensions.Split(';')){if($extension){$names+=($Command+$extension)}}
    }
    # Native application discovery does not need PowerShell's filesystem drive
    # provider or module discovery. Read PATH afresh so per-process changes are
    # honored even after earlier command discovery cached a missing executable.
    $searchPath=[Environment]::GetEnvironmentVariable('PATH','Process')
    foreach($directory in $searchPath.Split([IO.Path]::PathSeparator)){
        if(!$directory){continue}
        $directory=[Environment]::ExpandEnvironmentVariables($directory.Trim('"'))
        foreach($name in $names){
            try{$candidate=[IO.Path]::Combine($directory,$name)}catch{continue}
            if([IO.File]::Exists($candidate)){return [IO.Path]::GetFullPath($candidate)}
        }
    }
    throw "Executable was not found on PATH: $Command"
}

function Get-UICreatorCMakeTools {
    param([string]$Executable='')
    if(!$Executable){$Executable='cmake.exe'}
    try {
        $cmakePath=Resolve-UICreatorApplicationPath -Command $Executable
    }catch{throw "CMake executable was not found: $Executable. Pass -CMakeExecutable with the full path to cmake.exe."}
    function Read-ToolVersion([string]$Path,[string]$Name){
        if(!(Test-Path -LiteralPath $Path -PathType Leaf)){throw "The selected CMake toolset is incomplete: missing $Path"}
        $versionOutput=@(& $Path --version 2>&1)
        if($LASTEXITCODE -ne 0){throw "$Name --version failed for $Path ($LASTEXITCODE): $($versionOutput -join ' ')"}
        $firstLine=[string]($versionOutput | Select-Object -First 1)
        if($firstLine -notmatch ('^'+[regex]::Escape($Name)+' version ([0-9]+\.[0-9]+\.[0-9]+)(?:-[^\s]+)?\s*$')){
            throw "Cannot identify $Name version from $Path : $firstLine"
        }
        return [version]$Matches[1]
    }
    $cmakeVersion=Read-ToolVersion $cmakePath 'cmake'
    if($cmakeVersion -lt [version]'3.24.0'){throw "CMake 3.24 or newer is required; selected $cmakeVersion at $cmakePath"}
    $toolsDirectory=Split-Path $cmakePath -Parent
    $ctestPath=Join-Path $toolsDirectory 'ctest.exe';$cpackPath=Join-Path $toolsDirectory 'cpack.exe'
    foreach($tool in @(@{Name='ctest';Path=$ctestPath},@{Name='cpack';Path=$cpackPath})){
        $toolVersion=Read-ToolVersion $tool.Path $tool.Name
        if($toolVersion -ne $cmakeVersion){throw "CMake toolset versions differ: cmake $cmakeVersion, $($tool.Name) $toolVersion. Use a complete CMake installation."}
    }
    Write-Host "Using CMake $cmakeVersion toolset: $toolsDirectory"
    return [pscustomobject]@{CMake=$cmakePath;CTest=$ctestPath;CPack=$cpackPath;Version=$cmakeVersion.ToString()}
}

function Get-UICreatorNsisTool {
    param([Parameter(Mandatory=$true)][string]$Executable)
    try{
        $path=Resolve-UICreatorApplicationPath -Command $Executable
    }catch{throw "NSIS compiler was not found: $Executable. Pass -NsisExecutable with the full path to makensis.exe."}
    $output=@(& $path /VERSION 2>&1)
    if($LASTEXITCODE -ne 0){throw "NSIS /VERSION failed for $path ($LASTEXITCODE): $($output -join ' ')"}
    $versionText=([string]($output | Select-Object -First 1)).Trim()
    if($versionText -notmatch '^v?([0-9]+\.[0-9]+(?:\.[0-9]+)?)(?:[^0-9.].*)?$'){throw "Cannot identify NSIS compiler version from $path : $versionText"}
    $version=[version]$Matches[1]
    if($version -lt [version]'3.3'){throw "CPack requires NSIS 3.03 or newer; selected $version at $path"}
    Write-Host "Using NSIS $version compiler: $path"
    return [pscustomobject]@{Executable=$path;Version=$version.ToString()}
}
