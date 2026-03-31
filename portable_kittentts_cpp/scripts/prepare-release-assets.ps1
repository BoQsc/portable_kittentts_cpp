[CmdletBinding()]
param(
    [string[]]$Models = @("all"),
    [string]$ZigVersion = "0.15.2",
    [string]$OnnxRuntimeVersion = "1.24.4",
    [string]$EspeakNgVersion = "1.52.0"
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path $PSScriptRoot -Parent

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

function Invoke-Download {
    param(
        [Parameter(Mandatory = $true)][string]$Uri,
        [Parameter(Mandatory = $true)][string]$OutFile
    )

    for ($attempt = 1; $attempt -le 3; $attempt++) {
        try {
            Invoke-WebRequest -Uri $Uri -OutFile $OutFile
            return
        }
        catch {
            if ($attempt -eq 3) {
                throw
            }
            Start-Sleep -Seconds 2
        }
    }
}

function Expand-ZipLikeArchive {
    param(
        [Parameter(Mandatory = $true)][string]$ArchivePath,
        [Parameter(Mandatory = $true)][string]$DestinationPath
    )

    Ensure-Directory $DestinationPath

    $archiveToUse = $ArchivePath
    $temporaryCopy = $null
    if ([System.IO.Path]::GetExtension($ArchivePath).ToLowerInvariant() -ne ".zip") {
        $temporaryCopy = Join-Path $env:TEMP (([System.IO.Path]::GetFileNameWithoutExtension($ArchivePath)) + "-" + [guid]::NewGuid().ToString("N") + ".zip")
        Copy-Item -LiteralPath $ArchivePath -Destination $temporaryCopy -Force
        $archiveToUse = $temporaryCopy
    }

    Expand-Archive -LiteralPath $archiveToUse -DestinationPath $DestinationPath -Force

    if ($temporaryCopy) {
        Remove-Item $temporaryCopy -Force
    }
}

function Install-Zig {
    $zigRoot = Join-Path $repoRoot "tools\zig"
    $zigExe = Join-Path $zigRoot "zig.exe"
    if (Test-Path $zigExe) {
        return
    }

    Reset-Directory $zigRoot

    $zigArchive = Join-Path $env:TEMP ("zig-" + $ZigVersion + ".zip")
    $zigUrl = "https://ziglang.org/download/$ZigVersion/zig-x86_64-windows-$ZigVersion.zip"
    Invoke-Download -Uri $zigUrl -OutFile $zigArchive

    $zigExtract = Join-Path $env:TEMP ("zig-" + $ZigVersion + "-" + [guid]::NewGuid().ToString("N"))
    Ensure-Directory $zigExtract
    Expand-Archive -LiteralPath $zigArchive -DestinationPath $zigExtract -Force

    $zigExeItem = Get-ChildItem -Path $zigExtract -Recurse -Filter "zig.exe" | Select-Object -First 1
    if (-not $zigExeItem) {
        throw "Zig archive did not contain zig.exe: $zigExtract"
    }

    $zigSourceRoot = $zigExeItem.Directory.FullName
    Copy-Item -Path (Join-Path $zigSourceRoot "*") -Destination $zigRoot -Recurse -Force

    Remove-Item $zigArchive -Force
    Remove-Item $zigExtract -Recurse -Force
}

function Install-OnnxRuntime {
    $ortRoot = Join-Path $repoRoot "third_party\onnxruntime"
    $ortIncludeRoot = Join-Path $ortRoot "include"
    $ortLibRoot = Join-Path $ortRoot "lib"
    $ortRuntimeRoot = Join-Path $repoRoot "runtime\onnxruntime"

    Reset-Directory $ortIncludeRoot
    Reset-Directory $ortLibRoot
    Reset-Directory $ortRuntimeRoot

    $ortPackage = Join-Path $env:TEMP ("microsoft.ml.onnxruntime-" + $OnnxRuntimeVersion + ".nupkg")
    $ortUrl = "https://api.nuget.org/v3-flatcontainer/microsoft.ml.onnxruntime/$OnnxRuntimeVersion/microsoft.ml.onnxruntime.$OnnxRuntimeVersion.nupkg"
    Invoke-Download -Uri $ortUrl -OutFile $ortPackage

    $ortExtract = Join-Path $env:TEMP ("ort-" + $OnnxRuntimeVersion + "-" + [guid]::NewGuid().ToString("N"))
    Ensure-Directory $ortExtract
    Expand-ZipLikeArchive -ArchivePath $ortPackage -DestinationPath $ortExtract

    Copy-Item -Path (Join-Path $ortExtract "build\native\include\*") -Destination $ortIncludeRoot -Recurse -Force
    Copy-Item -LiteralPath (Join-Path $ortExtract "runtimes\win-x64\native\onnxruntime.dll") -Destination $ortRuntimeRoot -Force
    Copy-Item -LiteralPath (Join-Path $ortExtract "runtimes\win-x64\native\onnxruntime_providers_shared.dll") -Destination $ortRuntimeRoot -Force
    Copy-Item -LiteralPath (Join-Path $ortExtract "runtimes\win-x64\native\onnxruntime.lib") -Destination $ortLibRoot -Force
    Copy-Item -LiteralPath (Join-Path $ortExtract "runtimes\win-x64\native\onnxruntime_providers_shared.lib") -Destination $ortLibRoot -Force

    Remove-Item $ortPackage -Force
    Remove-Item $ortExtract -Recurse -Force
}

function Install-EspeakNg {
    $espeakRoot = Join-Path $repoRoot "runtime\espeak-lite"
    Reset-Directory $espeakRoot

    $msiPath = Join-Path $env:TEMP ("espeak-ng-" + $EspeakNgVersion + ".msi")
    $msiUrls = @(
        "https://github.com/espeak-ng/espeak-ng/releases/download/$EspeakNgVersion/espeak-ng.msi",
        "https://github.com/espeak-ng/espeak-ng/releases/download/$EspeakNgVersion/espeak-ng-X64.msi",
        "https://github.com/espeak-ng/espeak-ng/releases/download/$EspeakNgVersion/espeak-ng-x64.msi"
    )
    $downloaded = $false
    foreach ($msiUrl in $msiUrls) {
        try {
            Invoke-Download -Uri $msiUrl -OutFile $msiPath
            $downloaded = $true
            break
        }
        catch {
            if (Test-Path $msiPath) {
                Remove-Item $msiPath -Force
            }
        }
    }
    if (-not $downloaded) {
        throw "Failed to download eSpeak NG MSI from any known release asset URL."
    }

    $extractRoot = Join-Path $env:TEMP ("espeak-ng-" + $EspeakNgVersion + "-" + [guid]::NewGuid().ToString("N"))
    Ensure-Directory $extractRoot

    $arguments = @(
        "/a",
        "`"$msiPath`"",
        "/qn",
        "TARGETDIR=`"$extractRoot`""
    )
    $process = Start-Process -FilePath "msiexec.exe" -ArgumentList $arguments -Wait -PassThru
    if ($process.ExitCode -ne 0) {
        throw "Failed to extract eSpeak NG MSI: exit code $($process.ExitCode)"
    }

    $library = Get-ChildItem -Path $extractRoot -Recurse -Filter "libespeak-ng.dll" | Select-Object -First 1
    if (-not $library) {
        throw "Could not find libespeak-ng.dll in the extracted eSpeak NG package."
    }

    $espeakSourceRoot = $library.Directory.FullName
    Copy-Item -Path (Join-Path $espeakSourceRoot "*") -Destination $espeakRoot -Recurse -Force

    Remove-Item $msiPath -Force
    Remove-Item $extractRoot -Recurse -Force
}

function Install-Model {
    param(
        [Parameter(Mandatory = $true)][string]$ModelKey,
        [Parameter(Mandatory = $true)][hashtable]$Spec
    )

    $modelRoot = Join-Path $repoRoot (Join-Path "model" $Spec.folder)
    Reset-Directory $modelRoot
    Ensure-Directory (Join-Path $modelRoot "onnx")
    Ensure-Directory (Join-Path $modelRoot "voices")

    $configUrl = "https://huggingface.co/$($Spec.repo)/resolve/main/config.json"
    $modelUrl = "https://huggingface.co/$($Spec.repo)/resolve/main/$($Spec.modelFile)"
    $voicesUrl = "https://huggingface.co/$($Spec.repo)/resolve/main/voices.npz"

    $configPath = Join-Path $modelRoot "config.json"
    $kittenConfigPath = Join-Path $modelRoot "kitten_config.json"
    $onnxPath = Join-Path $modelRoot "onnx\model.onnx"
    $voicesArchive = Join-Path $env:TEMP ("kitten-" + $ModelKey + "-voices.npz")

    Invoke-Download -Uri $configUrl -OutFile $configPath
    Copy-Item -LiteralPath $configPath -Destination $kittenConfigPath -Force
    Invoke-Download -Uri $modelUrl -OutFile $onnxPath
    Invoke-Download -Uri $voicesUrl -OutFile $voicesArchive

    Expand-ZipLikeArchive -ArchivePath $voicesArchive -DestinationPath (Join-Path $modelRoot "voices")
    Remove-Item $voicesArchive -Force
}

$modelSpecs = [ordered]@{
    "nano" = @{
        repo = "KittenML/kitten-tts-nano-0.8-fp32"
        folder = "kitten-nano-v0_8-onnx"
        modelFile = "kitten_tts_nano_v0_8.onnx"
    }
    "nano-int8" = @{
        repo = "KittenML/kitten-tts-nano-0.8-int8"
        folder = "kitten-nano-int8-v0_8-onnx"
        modelFile = "kitten_tts_nano_v0_8.onnx"
    }
    "micro" = @{
        repo = "KittenML/kitten-tts-micro-0.8"
        folder = "kitten-micro-v0_8-onnx"
        modelFile = "kitten_tts_micro_v0_8.onnx"
    }
    "mini" = @{
        repo = "KittenML/kitten-tts-mini-0.8"
        folder = "kitten-mini-v0_8-onnx"
        modelFile = "kitten_tts_mini_v0_8.onnx"
    }
}

$selectedModels = @()
if ($Models.Count -eq 1 -and $Models[0].ToLowerInvariant() -eq "all") {
    $selectedModels = @("nano", "nano-int8", "micro", "mini")
}
else {
    foreach ($model in $Models) {
        $selectedModels += $model.ToLowerInvariant()
    }
}

foreach ($model in $selectedModels) {
    if (-not ($modelSpecs.Keys -contains $model)) {
        throw "Unknown model variant: $model"
    }
}

Install-Zig
Install-OnnxRuntime
Install-EspeakNg

foreach ($model in $selectedModels) {
    Install-Model -ModelKey $model -Spec $modelSpecs[$model]
    Write-Host "Prepared model: $model"
}

Write-Host "Release assets prepared for: $($selectedModels -join ', ')"
