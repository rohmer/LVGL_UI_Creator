param([string]$JavaHome)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$directory = Join-Path $PSScriptRoot 'OpenNlp/dependencies'
New-Item -ItemType Directory -Force -Path $directory | Out-Null
$artifacts = @(
    'org/apache/opennlp/opennlp-tools/2.5.12/opennlp-tools-2.5.12.jar'
    foreach ($language in @('en', 'es', 'fr', 'pt', 'de', 'el')) {
        foreach ($task in @('sentdetect', 'tokenizer', 'pos')) {
            $name = "opennlp-models-$task-$language"
            "org/apache/opennlp/$name/1.3.0/$name-1.3.0.jar"
        }
    }
    'org/slf4j/slf4j-api/2.0.17/slf4j-api-2.0.17.jar'
    'org/slf4j/slf4j-nop/2.0.17/slf4j-nop-2.0.17.jar'
)

foreach ($artifact in $artifacts) {
    $uri = "https://repo.maven.apache.org/maven2/$artifact"
    $destination = Join-Path $directory ($artifact.Split('/')[-1])
    $checksum = (Invoke-RestMethod -Uri "$uri.sha1").Trim().Split(' ')[0]
    if ($checksum -notmatch '^[0-9a-fA-F]{40}$') { throw "Invalid checksum for $artifact" }
    if ((Test-Path -LiteralPath $destination) -and
        ((Get-FileHash -LiteralPath $destination -Algorithm SHA1).Hash -eq $checksum)) { continue }

    $temporary = "$destination.$([Guid]::NewGuid().ToString('N')).tmp"
    try {
        Invoke-WebRequest -UseBasicParsing -Uri $uri -OutFile $temporary
        if ((Get-FileHash -LiteralPath $temporary -Algorithm SHA1).Hash -ne $checksum) {
            throw "Checksum mismatch for $artifact"
        }
        Move-Item -LiteralPath $temporary -Destination $destination -Force
    }
    finally {
        if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary }
    }
}

if ($JavaHome) {
    $jdk = (Resolve-Path -LiteralPath $JavaHome).ProviderPath
    if (!(Test-Path -LiteralPath (Join-Path $jdk 'bin/java.exe') -PathType Leaf)) {
        throw 'JavaHome must point to a JDK directory containing bin/java.exe.'
    }
    Set-Content -LiteralPath (Join-Path $PSScriptRoot 'OpenNlp/java-home.txt') -Value $jdk -Encoding UTF8
}

Write-Output 'Apache OpenNLP dependencies are ready. Build Corpus Crawler before launching it.'
