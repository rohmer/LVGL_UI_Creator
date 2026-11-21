# Shared release validation. Read PE headers directly so validation also works
# on build hosts without dumpbin, and inspect native modules in SDK ZIP files.
function Get-UICreatorPeStreamInfo([IO.Stream]$Stream,[string]$Name,[bool]$Required=$false){
    $reader=[IO.BinaryReader]::new($Stream,[Text.Encoding]::ASCII,$true)
    try{
        if($Stream.Length -lt 2 -or $reader.ReadUInt16() -ne 0x5a4d){
            if($Required){throw "Not a Windows PE file: $Name"}
            return $null
        }
        if($Stream.Length -lt 64){throw "Truncated DOS header: $Name"}
        $Stream.Position=0x3c;$offset=[long]$reader.ReadUInt32()
        if($offset -lt 64 -or $offset -gt $Stream.Length-26){throw "Invalid PE header offset: $Name"}
        $Stream.Position=$offset
        if($reader.ReadUInt32() -ne 0x4550){throw "Invalid PE signature: $Name"}
        $machine=$reader.ReadUInt16()
        $Stream.Position=$offset+20;$optionalSize=$reader.ReadUInt16()
        if($optionalSize -lt 2 -or $offset+24+$optionalSize -gt $Stream.Length){throw "Truncated PE optional header: $Name"}
        $Stream.Position=$offset+24;$magic=$reader.ReadUInt16()
        if($magic -notin @(0x10b,0x20b)){throw "Unsupported PE optional header: $Name"}
        return [pscustomobject]@{machine=[int]$machine;bits=$(if($magic -eq 0x10b){32}else{64})}
    }finally{$reader.Dispose()}
}

function Get-UICreatorPeInfo([string]$Path,[switch]$Required){
    $stream=[IO.File]::OpenRead($Path)
    try{Get-UICreatorPeStreamInfo $stream $Path ([bool]$Required)}finally{$stream.Dispose()}
}

function Assert-UICreatorPeArchitecture {
    param([Parameter(Mandatory=$true)][string]$Root,
          [Parameter(Mandatory=$true)][ValidateSet('x64','x86')][string]$Architecture,
          [string[]]$IgnoreRelativePath=@(),[switch]$SkipArchives)
    $Root=[IO.Path]::GetFullPath($Root).TrimEnd([IO.Path]::DirectorySeparatorChar)
    if(!(Test-Path -LiteralPath $Root -PathType Container)){throw "PE payload directory is missing: $Root"}
    $expected=if($Architecture -eq 'x64'){0x8664}else{0x14c}
    $bits=if($Architecture -eq 'x64'){64}else{32}
    $nativeExtensions=@('.exe','.dll','.pyd','.node','.ocx','.cpl','.scr')
    $records=[Collections.Generic.List[object]]::new()
    function Add-CheckedPe([string]$Relative,$Information){
        if($null -eq $Information){return}
        if($Information.machine -ne $expected -or $Information.bits -ne $bits){
            throw ('Architecture mismatch in '+$Architecture+'/'+$Relative+': PE machine 0x'+$Information.machine.ToString('x4')+', '+$Information.bits+'-bit image; expected '+$Architecture+'.')
        }
        $records.Add([pscustomobject]@{path=$Relative;machine=('0x'+$Information.machine.ToString('x4'));bits=$Information.bits})
    }
    foreach($file in Get-ChildItem -LiteralPath $Root -Recurse -Force){
        if($file.Attributes -band [IO.FileAttributes]::ReparsePoint){throw "Release payload contains a reparse point: $($file.FullName)"}
        if($file.PSIsContainer){continue}
        $relative=$file.FullName.Substring($Root.Length+1).Replace('\','/')
        if($relative -in $IgnoreRelativePath){continue}
        Add-CheckedPe $relative (Get-UICreatorPeInfo $file.FullName -Required:($file.Extension.ToLowerInvariant() -in $nativeExtensions))
        if(!$SkipArchives -and $file.Extension -ieq '.zip'){
            Add-Type -AssemblyName System.IO.Compression.FileSystem
            $archive=[IO.Compression.ZipFile]::OpenRead($file.FullName)
            try{foreach($entry in $archive.Entries){
                if([IO.Path]::GetExtension($entry.FullName).ToLowerInvariant() -notin $nativeExtensions){continue}
                $source=$entry.Open();$memory=[IO.MemoryStream]::new()
                try{
                    $source.CopyTo($memory);$memory.Position=0
                    $label=$relative+'!/'+$entry.FullName.Replace('\','/')
                    Add-CheckedPe $label (Get-UICreatorPeStreamInfo $memory $label $true)
                }finally{$source.Dispose();$memory.Dispose()}
            }}finally{$archive.Dispose()}
        }
    }
    if(!$records.Count){throw "No native PE files found in $Root"}
    return [pscustomobject]@{architecture=$Architecture;files=@($records | Sort-Object path)}
}
