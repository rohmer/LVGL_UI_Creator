param(
    [Parameter(Mandatory=$true)][string]$RepositoryRoot,
    [Parameter(Mandatory=$true)][string]$CargoExecutable,
    [Parameter(Mandatory=$true)][string]$VcVars,
    [Parameter(Mandatory=$true)][ValidateSet('x86','x64')][string]$Architecture,
    [Parameter(Mandatory=$true)][string]$RustTarget,
    [Parameter(Mandatory=$true)][string]$OutputFile
)
$ErrorActionPreference='Stop'
$repository=[IO.Path]::GetFullPath($RepositoryRoot)
$cargo=(Get-Command $CargoExecutable -ErrorAction Stop).Source
if(Test-Path -LiteralPath (Join-Path $repository 'build\rustup\settings.toml')) {
    $env:RUSTUP_HOME=Join-Path $repository 'build\rustup'
    $env:CARGO_HOME=Join-Path $repository 'build\cargo'
}
# MSBuild can inherit a developer prompt and then add its own toolchain paths.
# Initialize a bounded environment instead of appending vcvars paths repeatedly.
$env:PATH="$env:SystemRoot\System32;$env:SystemRoot;$env:SystemRoot\System32\WindowsPowerShell\v1.0"
Get-ChildItem Env: | Where-Object { $_.Name -match '^_?_?VSCMD' } | ForEach-Object { Remove-Item -LiteralPath ('Env:'+$_.Name) }
$setup='call "{0}" {1} >nul && set' -f $VcVars,$Architecture
$environment=& $env:ComSpec /d /c $setup
if($LASTEXITCODE){throw 'Cannot initialize the Rust MSVC linker environment'}
foreach($line in $environment) {
    if($line -match '^([^=]+)=(.*)$') {[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}
}
& $cargo build --release --locked --target $RustTarget --manifest-path (Join-Path $repository '3rdParty\libimagequant\imagequant-sys\Cargo.toml') --target-dir (Join-Path $repository 'build\imagequant-msbuild')
if($LASTEXITCODE){throw 'Imagequant Rust build failed'}
New-Item -ItemType Directory -Force -Path ([IO.Path]::GetDirectoryName($OutputFile)) | Out-Null
Copy-Item -LiteralPath (Join-Path $repository "build\imagequant-msbuild\$RustTarget\release\imagequant_sys.lib") -Destination $OutputFile -Force
