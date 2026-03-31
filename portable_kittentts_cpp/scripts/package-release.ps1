[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("dist", "sfx")]
    [string]$Kind,

    [ValidateSet("nano", "nano-int8", "micro", "mini")]
    [string]$Model = "nano",

    [string]$Tag = "dev",
    [string]$OutputDir = (Join-Path $PSScriptRoot "..\release")
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path $PSScriptRoot -Parent
$distRoot = Join-Path $repoRoot "dist"
$outputRoot = Resolve-Path $OutputDir -ErrorAction SilentlyContinue
if (-not $outputRoot) {
    $outputRoot = New-Item -ItemType Directory -Force -Path $OutputDir | Select-Object -ExpandProperty FullName
}
else {
    $outputRoot = $outputRoot.Path
}

$artifactRoot = Join-Path $outputRoot "artifacts"
$workRoot = Join-Path $outputRoot "work"
$payloadRoot = Join-Path $workRoot "payload"
$bootstrapperBuildRoot = Join-Path $workRoot "bootstrapper"
$payloadZip = Join-Path $workRoot "payload.zip"

function Ensure-Directory {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-Path $Path)) {
        New-Item -ItemType Directory -Force -Path $Path | Out-Null
    }
}

function Reset-Directory {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (Test-Path $Path) {
        Remove-Item $Path -Recurse -Force
    }
    Ensure-Directory $Path
}

function Get-DistItems {
    return @(Get-ChildItem -Force -LiteralPath $distRoot | ForEach-Object { $_.FullName })
}

function Get-ZigPath {
    $zig = Get-Command zig -ErrorAction SilentlyContinue
    if ($zig) {
        return $zig.Source
    }

    $localZig = Join-Path $repoRoot "tools\zig\zig.exe"
    if (Test-Path $localZig) {
        return $localZig
    }

    throw "Zig not found. Install Zig 0.15+ or place zig.exe in tools\zig\zig.exe."
}

function Get-ModelFolderName {
    switch ($Model) {
        "nano" { return "kitten-nano-v0_8-onnx" }
        "nano-int8" { return "kitten-nano-int8-v0_8-onnx" }
        "micro" { return "kitten-micro-v0_8-onnx" }
        "mini" { return "kitten-mini-v0_8-onnx" }
        default { throw "Unsupported model: $Model" }
    }
}

function New-DistZip {
    Ensure-Directory $artifactRoot

    $artifactName = "portable_kittentts_cpp-$Tag-dist.zip"
    $artifactPath = Join-Path $artifactRoot $artifactName
    if (Test-Path $artifactPath) {
        Remove-Item $artifactPath -Force
    }

    $distItems = Get-DistItems
    if (-not $distItems) {
        throw "dist folder is empty or missing: $distRoot"
    }

    Compress-Archive -Path $distItems -DestinationPath $artifactPath -CompressionLevel Optimal
    Write-Host "Wrote $artifactPath"
}

function Build-Bootstrapper {
    Ensure-Directory $bootstrapperBuildRoot

    $zigPath = Get-ZigPath
    $bootstrapperSource = Join-Path $repoRoot "src\bootstrapper.cpp"
    $bootstrapperExe = Join-Path $bootstrapperBuildRoot "bootstrapper.exe"

    & $zigPath c++ -std=c++20 -Oz $bootstrapperSource -o $bootstrapperExe -lshell32 -Xlinker /subsystem:console
    if ($LASTEXITCODE -ne 0) {
        throw "Bootstrapper build failed."
    }

    return $bootstrapperExe
}

function Copy-PayloadFiles {
    param(
        [Parameter(Mandatory = $true)][string]$ModelFolderName
    )

    Reset-Directory $payloadRoot

    Copy-Item -Force (Join-Path $distRoot "kitten_tts.exe") (Join-Path $payloadRoot "kitten_tts.exe")
    Copy-Item -Force (Join-Path $distRoot "data") (Join-Path $payloadRoot "data") -Recurse
    Copy-Item -Force (Join-Path $distRoot "runtime") (Join-Path $payloadRoot "runtime") -Recurse

    $payloadModelRoot = Join-Path $payloadRoot "model"
    Ensure-Directory $payloadModelRoot
    Copy-Item -Force (Join-Path $distRoot "model\$ModelFolderName") (Join-Path $payloadModelRoot $ModelFolderName) -Recurse

    if (Test-Path (Join-Path $distRoot "LICENSE")) {
        Copy-Item -Force (Join-Path $distRoot "LICENSE") (Join-Path $payloadRoot "LICENSE")
    }
    if (Test-Path (Join-Path $distRoot "THIRD_PARTY_NOTICES.md")) {
        Copy-Item -Force (Join-Path $distRoot "THIRD_PARTY_NOTICES.md") (Join-Path $payloadRoot "THIRD_PARTY_NOTICES.md")
    }
}

function Append-Payload {
    param(
        [Parameter(Mandatory = $true)][string]$ArtifactPath,
        [Parameter(Mandatory = $true)][string]$PayloadZipPath,
        [Parameter(Mandatory = $true)][string]$ModelKey
    )

    $magic = [Text.Encoding]::ASCII.GetBytes("KTTSZIP1")
    if ($magic.Length -ne 8) {
        throw "Unexpected footer magic length."
    }

    $modelBytes = [Text.Encoding]::ASCII.GetBytes($ModelKey)
    if ($modelBytes.Length -gt 16) {
        throw "Model key is too long for the footer."
    }

    $footer = New-Object byte[] 32
    [Array]::Copy($magic, 0, $footer, 0, 8)
    [Array]::Copy([BitConverter]::GetBytes([UInt64](Get-Item $PayloadZipPath).Length), 0, $footer, 8, 8)
    [Array]::Copy($modelBytes, 0, $footer, 16, $modelBytes.Length)

    $payloadStream = [System.IO.File]::OpenRead($PayloadZipPath)
    $artifactStream = [System.IO.File]::Open($ArtifactPath, [System.IO.FileMode]::Append, [System.IO.FileAccess]::Write, [System.IO.FileShare]::Read)
    try {
        $buffer = New-Object byte[] (1024 * 1024)
        while (($read = $payloadStream.Read($buffer, 0, $buffer.Length)) -gt 0) {
            $artifactStream.Write($buffer, 0, $read)
        }
        $artifactStream.Write($footer, 0, $footer.Length)
    }
    finally {
        $payloadStream.Dispose()
        $artifactStream.Dispose()
    }
}

function New-SingleExePackage {
    Ensure-Directory $artifactRoot
    Reset-Directory $workRoot

    $artifactName = "portable_kittentts_cpp-$Tag-$Model.exe"
    $artifactPath = Join-Path $artifactRoot $artifactName
    $bootstrapperExe = Build-Bootstrapper
    $modelFolderName = Get-ModelFolderName

    Copy-PayloadFiles -ModelFolderName $modelFolderName
    Compress-Archive -Path (Join-Path $payloadRoot "*") -DestinationPath $payloadZip -CompressionLevel Optimal

    Copy-Item -Force $bootstrapperExe $artifactPath
    Append-Payload -ArtifactPath $artifactPath -PayloadZipPath $payloadZip -ModelKey $Model

    if (-not (Test-Path $artifactPath)) {
        throw "Expected release artifact was not created: $artifactPath"
    }

    Write-Host "Wrote $artifactPath"
}

if (-not (Test-Path $distRoot)) {
    throw "dist folder not found. Run build.ps1 first."
}

Reset-Directory $artifactRoot

switch ($Kind) {
    "dist" { New-DistZip }
    "sfx" { New-SingleExePackage }
}
