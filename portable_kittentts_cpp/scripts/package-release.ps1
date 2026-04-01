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

function Get-RelativeBundlePath {
    param(
        [Parameter(Mandatory = $true)][string]$BasePath,
        [Parameter(Mandatory = $true)][string]$FullPath
    )

    $relative = $FullPath.Substring($BasePath.Length).TrimStart('\')
    return ($relative -replace '\\', '/')
}

function Write-UInt32LE {
    param(
        [Parameter(Mandatory = $true)][System.IO.Stream]$Stream,
        [Parameter(Mandatory = $true)][UInt32]$Value
    )

    $bytes = [BitConverter]::GetBytes($Value)
    $Stream.Write($bytes, 0, $bytes.Length)
}

function Write-UInt64LE {
    param(
        [Parameter(Mandatory = $true)][System.IO.Stream]$Stream,
        [Parameter(Mandatory = $true)][UInt64]$Value
    )

    $bytes = [BitConverter]::GetBytes($Value)
    $Stream.Write($bytes, 0, $bytes.Length)
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
    $srcInclude = Join-Path $repoRoot "src"

    & $zigPath c++ -std=c++20 -Oz -I $srcInclude $bootstrapperSource -o $bootstrapperExe -lshell32 -Xlinker /subsystem:console
    if ($LASTEXITCODE -ne 0) {
        throw "Bootstrapper build failed."
    }

    return $bootstrapperExe
}

function Copy-FileIfExists {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    if (Test-Path $Source) {
        Copy-Item -Force $Source $Destination
    }
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

    Copy-FileIfExists -Source (Join-Path $distRoot "README.md") -Destination (Join-Path $payloadRoot "README.md")
    Copy-FileIfExists -Source (Join-Path $distRoot "run.bat") -Destination (Join-Path $payloadRoot "run.bat")
    Copy-FileIfExists -Source (Join-Path $distRoot "run.ps1") -Destination (Join-Path $payloadRoot "run.ps1")
    Copy-FileIfExists -Source (Join-Path $distRoot "nano.bat") -Destination (Join-Path $payloadRoot "nano.bat")
    Copy-FileIfExists -Source (Join-Path $distRoot "nano-int8.bat") -Destination (Join-Path $payloadRoot "nano-int8.bat")
    Copy-FileIfExists -Source (Join-Path $distRoot "micro.bat") -Destination (Join-Path $payloadRoot "micro.bat")
    Copy-FileIfExists -Source (Join-Path $distRoot "mini.bat") -Destination (Join-Path $payloadRoot "mini.bat")
    Copy-FileIfExists -Source (Join-Path $distRoot "LICENSE") -Destination (Join-Path $payloadRoot "LICENSE")
    Copy-FileIfExists -Source (Join-Path $distRoot "THIRD_PARTY_NOTICES.md") -Destination (Join-Path $payloadRoot "THIRD_PARTY_NOTICES.md")
}

function Append-RawBundle {
    param(
        [Parameter(Mandatory = $true)][string]$ArtifactPath,
        [Parameter(Mandatory = $true)][string]$ModelKey
    )

    $files = @(Get-ChildItem -LiteralPath $payloadRoot -Recurse -File | Sort-Object FullName)
    if (-not $files) {
        throw "Payload folder is empty: $payloadRoot"
    }

    $artifactStream = [System.IO.File]::Open($ArtifactPath, [System.IO.FileMode]::Append, [System.IO.FileAccess]::Write, [System.IO.FileShare]::Read)
    try {
        [UInt64]$payloadSize = 0
        $buffer = New-Object byte[] (1024 * 1024)

        foreach ($file in $files) {
            $relativePath = Get-RelativeBundlePath -BasePath $payloadRoot -FullPath $file.FullName
            $pathBytes = [Text.Encoding]::UTF8.GetBytes($relativePath)
            if ($pathBytes.Length -gt [UInt32]::MaxValue) {
                throw "Embedded path is too long: $relativePath"
            }

            Write-UInt32LE -Stream $artifactStream -Value ([UInt32]$pathBytes.Length)
            Write-UInt64LE -Stream $artifactStream -Value ([UInt64]$file.Length)
            $artifactStream.Write($pathBytes, 0, $pathBytes.Length)

            $inputStream = [System.IO.File]::Open($file.FullName, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read)
            try {
                while (($read = $inputStream.Read($buffer, 0, $buffer.Length)) -gt 0) {
                    $artifactStream.Write($buffer, 0, $read)
                }
            }
            finally {
                $inputStream.Dispose()
            }

            $payloadSize += [UInt64](12 + $pathBytes.Length) + [UInt64]$file.Length
        }

        $footer = New-Object byte[] 32
        $magic = [Text.Encoding]::ASCII.GetBytes("KTTSRAW1")
        [Array]::Copy($magic, 0, $footer, 0, 8)
        [Array]::Copy([BitConverter]::GetBytes([UInt64]$payloadSize), 0, $footer, 8, 8)

        $modelBytes = [Text.Encoding]::ASCII.GetBytes($ModelKey)
        if ($modelBytes.Length -gt 16) {
            throw "Model key is too long for the footer."
        }
        [Array]::Copy($modelBytes, 0, $footer, 16, $modelBytes.Length)
        $artifactStream.Write($footer, 0, $footer.Length)
    }
    finally {
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
    Copy-Item -Force $bootstrapperExe $artifactPath
    Append-RawBundle -ArtifactPath $artifactPath -ModelKey $Model

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
