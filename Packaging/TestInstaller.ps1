param([Parameter(Mandatory=$true)][string]$Installer,[Parameter(Mandatory=$true)][string]$StageRoot,[string]$TestRoot='',[switch]$SmokeApplications,[ValidateSet('combined','x64','x86')][string]$Architecture='combined',[string]$PythonExecutable='')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $PSScriptRoot 'PeTools.ps1')
. (Join-Path $PSScriptRoot 'CMakeTools.ps1')
$Installer=[IO.Path]::GetFullPath($Installer);$StageRoot=[IO.Path]::GetFullPath($StageRoot)
if(!$TestRoot){$TestRoot=Join-Path $repo ('build\windows-installer\smoke-'+[Guid]::NewGuid().ToString('N'))}
$TestRoot=[IO.Path]::GetFullPath($TestRoot)
if(!$TestRoot.StartsWith($repo+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Installer smoke-test directories must be inside this workspace.'}
if(Test-Path -LiteralPath $TestRoot){throw "Use a new empty test root: $TestRoot"}
New-Item -ItemType Directory -Path $TestRoot | Out-Null
$applicationResults=[Collections.Generic.List[object]]::new()
$iconResults=[Collections.Generic.List[object]]::new()
function NativeArgument([string]$Value){
    # Windows argv quoting, including quotes and trailing directory separators.
    return '"'+[regex]::Replace([regex]::Replace($Value,'(\\*)"','$1$1\"'),'(\\+)$','$1$1')+'"'
}
function SmokeProcess([string]$Executable,[string[]]$Arguments,[string]$Work,[string]$Name,[int]$TimeoutSeconds=30){
    if(!(Test-Path -LiteralPath $Executable -PathType Leaf)){throw "Installed smoke executable is missing: $Executable"}
    $stdout=Join-Path $Work ($Name+'.stdout.log');$stderr=Join-Path $Work ($Name+'.stderr.log')
    $argumentLine=($Arguments | ForEach-Object {NativeArgument $_}) -join ' '
    $savedEnvironment=@{}
    foreach($key in @('SDL_VIDEO_DRIVER','SDL_AUDIO_DRIVER','SDL_RENDER_DRIVER','PATH','PYTHONHOME','PYTHONPATH','NODE_PATH','NODE_OPTIONS','UICREATOR_SESSION_DIR')){$savedEnvironment[$key]=[Environment]::GetEnvironmentVariable($key,'Process')}
    try{
        $env:SDL_VIDEO_DRIVER='dummy';$env:SDL_AUDIO_DRIVER='dummy';$env:SDL_RENDER_DRIVER='software'
        $session=Join-Path $Work ('session-'+$Name+'-'+[Guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path $session | Out-Null
        $env:UICREATOR_SESSION_DIR=$session
        # Release applications must find their own interpreters and modules.
        # Do not allow an installed host Node/Python to mask missing payloads.
        $env:PATH=(Join-Path $env:SystemRoot 'System32')+';'+$env:SystemRoot
        foreach($key in @('PYTHONHOME','PYTHONPATH','NODE_PATH','NODE_OPTIONS')){[Environment]::SetEnvironmentVariable($key,$null,'Process')}
        $process=Start-Process -FilePath $Executable -ArgumentList $argumentLine -WorkingDirectory $Work -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        # Windows PowerShell can otherwise discard the exit status of a child
        # that finishes before WaitForExit; retain its process handle first.
        $ownedHandle=$process.Handle
    }finally{foreach($key in $savedEnvironment.Keys){[Environment]::SetEnvironmentVariable($key,$savedEnvironment[$key],'Process')}}
    $started=[DateTime]::UtcNow
    if(!$process.WaitForExit($TimeoutSeconds*1000)){
        try{$process.Kill();$process.WaitForExit(5000) | Out-Null}catch{}
        throw "Installed $Name smoke test exceeded $TimeoutSeconds seconds; process $($process.Id) was stopped. Logs: $Work"
    }
    $process.Refresh()
    $image=Get-UICreatorPeInfo $Executable -Required
    $applicationResults.Add(@{name=$Name;executable=$Executable;arguments=$Arguments;exitCode=$process.ExitCode;elapsedSeconds=([DateTime]::UtcNow-$started).TotalSeconds;imageBits=$image.bits;peMachine=('0x'+$image.machine.ToString('x4'));pathPolicy='windows-system-only';sessionDirectory=$session})
    if($process.ExitCode -ne 0){
        $details=((Get-Content -LiteralPath $stdout -Tail 8 -ErrorAction SilentlyContinue)+(Get-Content -LiteralPath $stderr -Tail 8 -ErrorAction SilentlyContinue)) -join "`n"
        throw "Installed $Name smoke test failed with exit $($process.ExitCode): $details"
    }
}
function SmokeInstalledMicroPythonExport([string]$Destination,[string]$Project,[string]$Protobuf,[string]$Work){
    foreach($frontEnd in @('codegen','compiler')){
        $microPython=Join-Path $Work ('generated-micropython-'+$frontEnd)
        if($frontEnd -eq 'codegen'){
            SmokeProcess (Join-Path $Destination 'UICreatorCodeGen.exe') @('--format','micropython',$Protobuf,$microPython) $Work 'micropython-codegen'
        }else{
            SmokeProcess (Join-Path $Destination 'LVGLCompiler.exe') @($Project,'--format','micropython','--target','9.3','--output',$microPython) $Work 'micropython-compiler'
        }
        foreach($name in @('ui.py','ui.json','uicreator_lvgl.py','main.py','README.md','lv_conf.defaults.h','export-manifest.json')){
            $file=Join-Path $microPython $name
            if(!(Test-Path -LiteralPath $file -PathType Leaf) -or (Get-Item -LiteralPath $file).Length -eq 0){throw "Installed MicroPython exporter omitted $name"}
        }
        $microManifest=Get-Content -LiteralPath (Join-Path $microPython 'export-manifest.json') -Raw | ConvertFrom-Json
        $microDocument=Get-Content -LiteralPath (Join-Path $microPython 'ui.json') -Raw | ConvertFrom-Json
        if($microManifest.format -ne 'micropython' -or $microManifest.lvglVersion -ne '9.3.0' -or $microManifest.binding.repository -ne 'https://github.com/lvgl/lv_binding_micropython'){
            throw 'Installed MicroPython exporter targets an unexpected binding.'
        }
        if($microDocument.screens[0].obj[0].button.text -ne 'Installed application smoke'){throw 'Installed MicroPython export lost the authored widget.'}
    }
}
function SmokeInstalledXmlExport([string]$Destination,[string]$Project,[string]$Protobuf,[string]$Work,[string]$ReferenceBitmap){
    $reference=[IO.File]::ReadAllBytes($ReferenceBitmap)
    foreach($frontEnd in @('codegen','compiler')){
        $xml=Join-Path $Work ('installed-smoke-'+$frontEnd+'.xml')
        if($frontEnd -eq 'codegen'){
            SmokeProcess (Join-Path $Destination 'UICreatorCodeGen.exe') @('--format','xml',$Protobuf,$xml) $Work 'xml-codegen'
        }else{
            SmokeProcess (Join-Path $Destination 'LVGLCompiler.exe') @($Project,'--format','xml','--target','9.6','--output',$xml) $Work 'xml-compiler'
        }
        if(!(Test-Path -LiteralPath $xml -PathType Leaf) -or (Get-Item -LiteralPath $xml).Length -eq 0){throw 'Installed XML exporter produced no document.'}
        $xmlDocument=[xml]([IO.File]::ReadAllText($xml))
        $root=$xmlDocument.DocumentElement
        if($root.LocalName -ne 'uicreator' -or $root.GetAttribute('format_version') -ne '1' -or $root.GetAttribute('kind') -ne 'project' -or $root.GetAttribute('type') -ne 'project'){
            throw 'Installed XML exporter produced an unexpected document format.'
        }
        $bitmap=Join-Path $Work ('viewer-xml-'+$frontEnd+'.bmp')
        SmokeProcess (Join-Path $Destination 'UICreatorViewer.exe') @($xml,'--frames','4','--screenshot',$bitmap) $Work ('xml-viewer-'+$frontEnd)
        $pixels=[IO.File]::ReadAllBytes($bitmap)
        if($pixels.Length -ne $reference.Length -or [Convert]::ToBase64String($pixels) -ne [Convert]::ToBase64String($reference)){
            throw 'Installed XML and Protobuf viewers rendered different framebuffers.'
        }
        $fromXml=Join-Path $Work ('from-xml-'+$frontEnd+'.pb')
        SmokeProcess (Join-Path $Destination 'LVGLCompiler.exe') @($xml,'--format','protobuf','--output',$fromXml) $Work ('xml-input-'+$frontEnd)
        if(!(Test-Path -LiteralPath $fromXml -PathType Leaf) -or (Get-Item -LiteralPath $fromXml).Length -eq 0){throw 'Installed compiler could not read the exported XML.'}
    }
}
function SmokeInstalledApplications([string]$Destination,[string]$Architecture){
    $work=Join-Path $TestRoot ('applications-'+$Architecture);New-Item -ItemType Directory -Path $work | Out-Null
    $project=Join-Path $work 'installed-smoke.json';$protobuf=Join-Path $work 'installed-smoke.pb';$generated=Join-Path $work 'generated-c';$bitmap=Join-Path $work 'viewer.bmp'
    $document=@{formatVersion=2;canvas=@{width=320;height=240};variables=@();obj=@(@{oid=0;par=-1;base=@{name='Installed smoke button';drag=$true;coords=@{x=24;y=32;width=240;height=56}};button=@{text='Installed application smoke'}})}
    [IO.File]::WriteAllText($project,($document | ConvertTo-Json -Depth 12),[Text.UTF8Encoding]::new($false))
    SmokeProcess (Join-Path $Destination 'UICreator.exe') @('--smoke-test',$project) $work 'editor'
    SmokeProcess (Join-Path $Destination 'UICreatorCodeGen.exe') @($project,$generated) $work 'codegen'
    foreach($name in @('ui.c','ui.h','ui_assets.c','manifest.json','CMakeLists.txt')){if(!(Test-Path -LiteralPath (Join-Path $generated $name) -PathType Leaf)){throw "Installed code generator omitted $name"}}
    SmokeProcess (Join-Path $Destination 'LVGLCompiler.exe') @($project,'--format','protobuf','--target','9.6','--output',$protobuf) $work 'compiler'
    if(!(Test-Path -LiteralPath $protobuf -PathType Leaf) -or (Get-Item -LiteralPath $protobuf).Length -eq 0){throw 'Installed compiler did not write a Protobuf project.'}
    SmokeInstalledMicroPythonExport $Destination $project $protobuf $work
    SmokeProcess (Join-Path $Destination 'UICreatorViewer.exe') @($protobuf,'--frames','4','--screenshot',$bitmap) $work 'viewer'
    $pixels=[IO.File]::ReadAllBytes($bitmap)
    if($pixels.Length -lt 54 -or $pixels[0] -ne 0x42 -or $pixels[1] -ne 0x4d -or [BitConverter]::ToInt32($pixels,18) -ne 320 -or [Math]::Abs([BitConverter]::ToInt32($pixels,22)) -ne 240){throw 'Installed viewer did not render the expected 320x240 framebuffer.'}
    SmokeInstalledXmlExport $Destination $project $protobuf $work $bitmap
    $trie=Join-Path $work 'language-model'
    SmokeProcess (Join-Path $Destination 'UICreator.exe') @('--language-model-smoke',$trie) $work 'language-model'
    $languageReport=Get-Content -LiteralPath (Join-Path $trie 'report.json') -Raw | ConvertFrom-Json
    $expectedBits=if($Architecture -eq 'x86'){32}else{64}
    if($languageReport.status -ne 'passed' -or $languageReport.pointerBits -ne $expectedBits -or $languageReport.trieBytes -le 0){throw 'Installed language model generation/prediction/autocorrect smoke did not pass for the selected architecture.'}

    $fontJob=Join-Path $work 'font conversion';New-Item -ItemType Directory -Path $fontJob | Out-Null
    $fontSource=Join-Path $repo '3rdParty/LVGL/lvgl/src/libs/freetype/LiberationSans-Regular.ttf'
    if(!(Test-Path -LiteralPath $fontSource -PathType Leaf)){throw "Missing font input fixture: $fontSource"}
    $fontInput=Join-Path $fontJob ('font with spaces '+[char]0x65e5+[char]0x672c+'.ttf');Copy-Item -LiteralPath $fontSource -Destination $fontInput
    $fontRequest=Join-Path $fontJob 'request.json'
    $fontArguments=@('--font',$fontInput,'--range','0x20-0x7E','--size','16','--bpp','4','--format','lvgl','--output',(Join-Path $fontJob 'font.c'),'--lv-font-name','installed_font','--lv-include','lvgl.h')
    $previewArguments=@('--font',$fontInput,'--range','0x20-0x7E','--size','16','--bpp','4','--format','bin','--output','preview.bin','--no-compress')
    [IO.File]::WriteAllText($fontRequest,(@{args=$fontArguments;previewArgs=$previewArguments}|ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
    SmokeProcess (Join-Path $Destination 'UICreatorConverter.exe') @('font','--request',$fontRequest) $work 'font-converter' 60
    $fontResult=Get-Content -LiteralPath (Join-Path $fontJob 'result.json') -Raw | ConvertFrom-Json
    if(!@($fontResult.files).Count -or (Get-Item -LiteralPath (Join-Path $fontJob 'preview.bin')).Length -le 0){throw 'Installed font converter produced no LVGL output or preview.'}
    foreach($output in $fontResult.files){if((Get-Item -LiteralPath (Join-Path $fontJob $output.staged)).Length -le 0){throw 'Installed font converter output is empty.'}}

    $imageJob=Join-Path $work 'image conversion';New-Item -ItemType Directory -Path $imageJob | Out-Null
    $svg=Join-Path $imageJob ('image with spaces '+[char]0x65e5+[char]0x672c+'.svg')
    [IO.File]::WriteAllText($svg,'<svg xmlns="http://www.w3.org/2000/svg" width="8" height="6"><rect width="8" height="6" fill="#20a0c0"/><circle cx="4" cy="3" r="2" fill="#f06040"/></svg>',[Text.UTF8Encoding]::new($false))
    $imageRequest=Join-Path $imageJob 'request.json'
    [IO.File]::WriteAllText($imageRequest,(@{sources=@($svg);options=@{colorFormat='ARGB8888';format='BIN';compression='LZ4';align=1}}|ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
    SmokeProcess (Join-Path $Destination 'UICreatorConverter.exe') @('image','--request',$imageRequest) $work 'image-converter' 60
    $imageResult=Get-Content -LiteralPath (Join-Path $imageJob 'result.json') -Raw | ConvertFrom-Json
    if(!@($imageResult.assets).Count -or !@($imageResult.files).Count -or $imageResult.assets[0].deviceFrames[0].w -ne 8 -or $imageResult.assets[0].deviceFrames[0].h -ne 6){throw 'Installed SVG/image/LZ4 converter produced no expected LVGL frame.'}
    foreach($output in $imageResult.files){if((Get-Item -LiteralPath (Join-Path $imageJob $output.staged)).Length -le 12){throw 'Installed image converter output is incomplete.'}}

    $creatorJob=Join-Path $work 'project creation';New-Item -ItemType Directory -Path $creatorJob | Out-Null
    $uiSource=Join-Path $creatorJob 'ui';New-Item -ItemType Directory -Path $uiSource | Out-Null
    Copy-Item -LiteralPath $project -Destination (Join-Path $uiSource 'project.json');Copy-Item -LiteralPath $protobuf -Destination (Join-Path $uiSource 'project.pb')
    $catalog=Get-Content -LiteralPath (Join-Path $Destination 'project-creator/catalog.json') -Raw | ConvertFrom-Json
    $template=@($catalog | Where-Object uicreatorTemplate -eq 'raspberry-pi-linux')[0]
    if(!$template){throw 'Installed project catalog is missing the offline Linux template.'}
    $created=Join-Path $creatorJob 'new project'
    $creatorRequest=Join-Path $creatorJob 'request.json'
    $request=@{operation='create';manifest=$template;destination=$created;project=$document;format='protobuf';lvglVersion='template';cache=(Join-Path $creatorJob 'cache');uiSourceFolder=$uiSource;linuxToolkitArchive=(Join-Path $Destination 'project-creator/linux-toolkit.zip')}
    [IO.File]::WriteAllText($creatorRequest,($request|ConvertTo-Json -Depth 24),[Text.UTF8Encoding]::new($false))
    SmokeProcess (Join-Path $Destination 'project-creator/UICreatorProjectTool.exe') @('--request',$creatorRequest) $work 'project-creator' 120
    $creatorResult=Get-Content -LiteralPath (Join-Path $creatorJob 'result.json') -Raw | ConvertFrom-Json
    if(!$creatorResult.linuxBuild){throw 'Installed project creator did not produce a Linux build.'}
    foreach($name in @('CMakeLists.txt','ui/project.pb','embedded/features.json','embedded/config/lv_conf.h','platform/Runtime/linux/Viewer.cpp')){if(!(Test-Path -LiteralPath (Join-Path $created $name) -PathType Leaf)){throw "Installed project creator omitted $name"}}
    $vendorSource=Join-Path $repo '3rdParty/LVGL/lvgl/src/display/lv_display.c'
    $vendorHash=(Get-FileHash -LiteralPath $vendorSource -Algorithm SHA256).Hash
    if((Get-FileHash -LiteralPath (Join-Path $created 'embedded/vendor/3rdParty/LVGL/lvgl/src/display/lv_display.c') -Algorithm SHA256).Hash -ne $vendorHash){throw 'Installed Linux project creator used an outdated LVGL source.'}
    $catalogRequest=Join-Path $creatorJob 'catalog-request.json'
    [IO.File]::WriteAllText($catalogRequest,(@{operation='catalog';cache=(Join-Path $creatorJob 'cache')}|ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
    SmokeProcess (Join-Path $Destination 'project-creator/UICreatorProjectTool.exe') @('--request',$catalogRequest) $work 'project-catalog' 60
    $runtimeCatalog=(Get-Content -LiteralPath (Join-Path $creatorJob 'result.json') -Raw | ConvertFrom-Json).catalog
    foreach($board in @('pico','pico2')){
        $identity=if($board -eq 'pico'){'raspberry-pi-pico'}else{'raspberry-pi-pico2'}
        $picoTemplate=@($runtimeCatalog | Where-Object uicreatorTemplate -eq $identity)[0]
        if(!$picoTemplate){throw "Installed project catalog is missing the $board template."}
        $picoJob=Join-Path $creatorJob $board;New-Item -ItemType Directory -Path $picoJob | Out-Null
        $picoProject=Join-Path $picoJob 'new project';$picoRequest=Join-Path $picoJob 'request.json'
        $request=@{operation='create';manifest=$picoTemplate;destination=$picoProject;project=$document;format='protobuf';lvglVersion='template';cache=(Join-Path $creatorJob 'cache');uiSourceFolder=$uiSource;picoToolkitArchive=(Join-Path $Destination 'project-creator/pico-toolkit.zip')}
        [IO.File]::WriteAllText($picoRequest,($request|ConvertTo-Json -Depth 24),[Text.UTF8Encoding]::new($false))
        SmokeProcess (Join-Path $Destination 'project-creator/UICreatorProjectTool.exe') @('--request',$picoRequest) $work ('project-creator-'+$board) 120
        $picoResult=Get-Content -LiteralPath (Join-Path $picoJob 'result.json') -Raw | ConvertFrom-Json
        if($picoResult.picoBuild.board -ne $board -or $picoResult.picoBuild.format -ne 'protobuf'){throw "Installed $board project creator selected the wrong target."}
        foreach($name in @('CMakeLists.txt','ui/project.pb','embedded/features.json','embedded/config/lv_conf.h','src/lvgl_pb_pico.cpp','lvgl-pb-pico-sdk.json')){
            if(!(Test-Path -LiteralPath (Join-Path $picoProject $name) -PathType Leaf)){throw "Installed $board project creator omitted $name"}
        }
        $picoMetadata=Get-Content -LiteralPath (Join-Path $picoProject 'lvgl-pb-pico-sdk.json') -Raw | ConvertFrom-Json
        if($picoMetadata.board -ne $board){throw "Installed $board project metadata uses the wrong board."}
        if((Get-FileHash -LiteralPath (Join-Path $picoProject 'embedded/vendor/3rdParty/LVGL/lvgl/src/display/lv_display.c') -Algorithm SHA256).Hash -ne $vendorHash){throw "Installed $board project creator used an outdated LVGL source."}
    }
    Write-Output "PASS: installed $Architecture editor/compiler/codegen/viewer, MicroPython/XML export and loading, trie prediction/autocorrect, font, SVG/image compression and offline Linux/Pico/Pico2 project conversion."
}
function RegistrationSnapshot {
    $state=@{}
    foreach($view in @([Microsoft.Win32.RegistryView]::Registry32,[Microsoft.Win32.RegistryView]::Registry64)){
        $hive=[Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::CurrentUser,$view)
        try{foreach($application in @('LVGL UI Creator','LVGL UI Creator x64','LVGL UI Creator x86')){
            $item=$hive.OpenSubKey('Software\Microsoft\Windows\CurrentVersion\Uninstall\'+$application)
            try{if($item){foreach($name in $item.GetValueNames()){$state['reg/'+$view+'/'+$application+'/'+$name]=[string]$item.GetValue($name)}}}finally{if($item){$item.Dispose()}}
        }}finally{$hive.Dispose()}
    }
    foreach($application in @('LVGL UI Creator','LVGL UI Creator x64','LVGL UI Creator x86')){
        $shortcuts=Join-Path ([Environment]::GetFolderPath('Programs')) $application
        if(Test-Path -LiteralPath $shortcuts){foreach($file in Get-ChildItem -LiteralPath $shortcuts -File -Recurse){$state['shortcut/'+$file.FullName]=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash}}
    }
    return ($state.GetEnumerator() | Sort-Object Key | ConvertTo-Json -Compress)
}
$before=RegistrationSnapshot
$selections=if($Architecture -eq 'combined'){@('default','x64','x86')}else{@($Architecture)}
foreach($selection in $selections){
    $destination=Join-Path $TestRoot $selection
    $archArgument=if($Architecture -eq 'combined' -and $selection -ne 'default'){" /ARCH=$selection"}else{''}
    $process=Start-Process -FilePath $Installer -ArgumentList "/S /TESTMODE=1$archArgument /D=$destination" -WindowStyle Hidden -Wait -PassThru
    if($process.ExitCode){throw "Installer failed ($selection): $($process.ExitCode)"}
    $expected=if($selection -eq 'default'){if([Environment]::Is64BitOperatingSystem){'x64'}else{'x86'}}else{$selection}
    $manifest=Get-Content -LiteralPath (Join-Path $StageRoot "$expected/architecture-manifest.json") -Raw | ConvertFrom-Json
    $installed=Get-Content -LiteralPath (Join-Path $destination 'architecture-manifest.json') -Raw | ConvertFrom-Json
    if($installed.architecture -ne $expected){throw "Wrong architecture installed for $selection"}
    $installedPe=Assert-UICreatorPeArchitecture -Root $destination -Architecture $expected -IgnoreRelativePath @('Uninstall.exe')
    $uninstallerPe=Get-UICreatorPeInfo (Join-Path $destination 'Uninstall.exe') -Required
    if($uninstallerPe.machine -ne 0x14c -or $uninstallerPe.bits -ne 32){throw 'NSIS uninstaller must remain compatible with 32-bit Windows.'}
    $installedPe | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $TestRoot ('pe-audit-'+$selection+'.json')) -Encoding UTF8
    foreach($entry in $manifest.files.PSObject.Properties){
        $path=Join-Path $destination $entry.Name
        if(!(Test-Path -LiteralPath $path -PathType Leaf) -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $entry.Value){throw "Installed file mismatch: $selection/$($entry.Name)"}
    }
    $other=if($expected -eq 'x64'){'x86'}else{'x64'}
    $otherManifestPath=Join-Path $StageRoot "$other/architecture-manifest.json"
    if(Test-Path -LiteralPath $otherManifestPath){
        $otherManifest=Get-Content -LiteralPath $otherManifestPath -Raw | ConvertFrom-Json
        foreach($entry in $otherManifest.files.PSObject.Properties){
            if(!$manifest.files.PSObject.Properties[$entry.Name] -and (Test-Path -LiteralPath (Join-Path $destination $entry.Name))){throw "Unselected architecture file was installed: $($entry.Name)"}
        }
    }
    if($SmokeApplications -and $selection -ne 'default'){
        $python=Resolve-UICreatorApplicationPath $(if($PythonExecutable){$PythonExecutable}else{'python.exe'})
        $iconReport=Join-Path $TestRoot ('icons-'+$selection+'.json')
        & $python (Join-Path $PSScriptRoot 'VerifyIcon.py') --icon (Join-Path $PSScriptRoot 'UICreator.ico') --executable $Installer --executable (Join-Path $destination 'UICreator.exe') --executable (Join-Path $destination 'UICreatorViewer.exe') --executable (Join-Path $destination 'Uninstall.exe') --report $iconReport
        if($LASTEXITCODE){throw "Installed $selection application/setup/uninstaller icon verification failed."}
        $iconResults.Add(@{architecture=$expected;report=$iconReport;passed=$true})
        SmokeInstalledApplications $destination $expected
    }
    $userFile=Join-Path $destination 'user-project-must-survive.txt';[IO.File]::WriteAllText($userFile,'Preserve user files.')
    if($selection -eq 'default'){
        $switch=if($expected -eq 'x64'){'x86'}else{'x64'}
        $upgrade=Start-Process -FilePath $Installer -ArgumentList "/S /TESTMODE=1 /ARCH=$switch /D=$destination" -WindowStyle Hidden -Wait -PassThru
        if($upgrade.ExitCode){throw "Architecture switch failed: $($upgrade.ExitCode)"}
        $manifest=Get-Content -LiteralPath (Join-Path $StageRoot "$switch/architecture-manifest.json") -Raw | ConvertFrom-Json
        foreach($entry in $manifest.files.PSObject.Properties){if((Get-FileHash -LiteralPath (Join-Path $destination $entry.Name) -Algorithm SHA256).Hash -ne $entry.Value){throw "Architecture switch file mismatch: $($entry.Name)"}}
        foreach($entry in $installed.files.PSObject.Properties){if(!$manifest.files.PSObject.Properties[$entry.Name] -and (Test-Path -LiteralPath (Join-Path $destination $entry.Name))){throw "Architecture switch left an old file: $($entry.Name)"}}
        Write-Output "PASS: switching an existing $expected installation to $switch preserves user files and removes the old package."
    }
    $uninstaller=Join-Path $destination 'Uninstall.exe'
    $uninstall=Start-Process -FilePath $uninstaller -ArgumentList '/S' -WindowStyle Hidden -Wait -PassThru
    if($uninstall.ExitCode){throw "Uninstaller failed ($selection): $($uninstall.ExitCode)"}
    $deadline=[DateTime]::UtcNow.AddSeconds(20)
    while((Test-Path -LiteralPath (Join-Path $destination 'installation.ini')) -and [DateTime]::UtcNow -lt $deadline){Start-Sleep -Milliseconds 100}
    if((Get-Content -LiteralPath $userFile -Raw) -ne 'Preserve user files.'){throw 'Uninstaller modified an unrelated user file.'}
    foreach($entry in $manifest.files.PSObject.Properties){if(Test-Path -LiteralPath (Join-Path $destination $entry.Name)){throw "Uninstaller left a packaged file: $($entry.Name)"}}
    if((RegistrationSnapshot) -ne $before){throw 'Isolated installer test changed production registration or shortcuts.'}
    Write-Output "PASS: $selection selects $expected; all hashes match; uninstall preserves user files; registration untouched."
}
if($Architecture -eq 'combined'){
    $invalid=Start-Process -FilePath $Installer -ArgumentList "/S /TESTMODE=1 /ARCH=invalid /D=$(Join-Path $TestRoot 'invalid')" -WindowStyle Hidden -Wait -PassThru
    if($invalid.ExitCode -ne 2){throw "Invalid architecture was accepted: $($invalid.ExitCode)"}
}
if($SmokeApplications){
    $hostInformation=@{osVersion=[Environment]::OSVersion.VersionString;os64Bit=[Environment]::Is64BitOperatingSystem;harnessProcessBits=([IntPtr]::Size*8);architecture=$env:PROCESSOR_ARCHITECTURE}
    @{version=1;passed=$true;host=$hostInformation;applications=$applicationResults.ToArray();icons=$iconResults.ToArray();workerPathPolicy='Windows System32 and Windows directories only; Python/Node override variables cleared; editor sessions isolated per process';native32BitOperatingSystemTested=(!$hostInformation.os64Bit)} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $TestRoot 'application-smoke.json') -Encoding UTF8
}
Write-Output "Installer smoke tests passed: $TestRoot"
