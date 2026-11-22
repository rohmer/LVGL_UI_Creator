param([Parameter(Mandatory=$true)][string]$BinaryDirectory,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
. (Join-Path $repo 'Packaging/PeTools.ps1')
$parseErrors=$null;$parseTokens=$null
$tree=[System.Management.Automation.Language.Parser]::ParseFile((Join-Path $repo 'Packaging/TestInstaller.ps1'),[ref]$parseTokens,[ref]$parseErrors)
if($parseErrors.Count){throw 'The installer smoke script has syntax errors.'}
# Load the same checks used after an actual install, without running an installer.
foreach($name in @('NativeArgument','SmokeProcess','SmokeInstalledMicroPythonExport')){
    $definition=$tree.Find({param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name},$false)
    if(!$definition){throw "Missing installed application check: $name"}
    . ([scriptblock]::Create($definition.Extent.Text))
}
$applicationResults=[Collections.Generic.List[object]]::new()
$work=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $work -Force | Out-Null
$project=Join-Path $work 'installed-smoke.json';$protobuf=Join-Path $work 'installed-smoke.pb'
$document=@{formatVersion=2;canvas=@{width=320;height=240};variables=@();obj=@(@{oid=0;par=-1;base=@{coords=@{x=24;y=32;width=240;height=56}};button=@{text='Installed application smoke'}})}
[IO.File]::WriteAllText($project,($document | ConvertTo-Json -Depth 12),[Text.UTF8Encoding]::new($false))
SmokeProcess (Join-Path $BinaryDirectory 'LVGLCompiler.exe') @($project,'--format','protobuf','--output',$protobuf) $work 'compiler-input'
SmokeInstalledMicroPythonExport $BinaryDirectory $project $protobuf $work
$applicationResults | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $work 'process-results.json') -Encoding UTF8
Write-Output 'Installed MicroPython CLI checks passed using the Windows-system-only PATH policy.'
