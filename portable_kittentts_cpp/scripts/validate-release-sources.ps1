[CmdletBinding()]
param(
    [string]$ZigVersion = "0.15.2",
    [string]$OnnxRuntimeVersion = "1.24.4",
    [string]$EspeakNgVersion = "1.52.0"
)

$ErrorActionPreference = "Stop"

function Test-Url {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Url
    )

    Write-Host "Checking $Name..."
    $httpCode = & curl.exe -I -L -s -o NUL -w "%{http_code}" $Url
    if ($LASTEXITCODE -ne 0 -or $httpCode -ne "200") {
        throw "$Name is not reachable: HTTP $httpCode - $Url"
    }
    Write-Host "$Name OK"
}

function Test-FirstAvailableUrl {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string[]]$Urls
    )

    foreach ($url in $Urls) {
        Write-Host "Checking $Name candidate: $url"
        $httpCode = & curl.exe -I -L -s -o NUL -w "%{http_code}" $url
        if ($LASTEXITCODE -eq 0 -and $httpCode -eq "200") {
            Write-Host "$Name OK"
            return
        }
    }

    throw "$Name is not reachable via any known release asset URL."
}

Test-Url -Name "Zig" -Url "https://ziglang.org/download/$ZigVersion/zig-x86_64-windows-$ZigVersion.zip"
Test-Url -Name "ONNX Runtime" -Url "https://api.nuget.org/v3-flatcontainer/microsoft.ml.onnxruntime/$OnnxRuntimeVersion/microsoft.ml.onnxruntime.$OnnxRuntimeVersion.nupkg"
Test-FirstAvailableUrl -Name "eSpeak NG" -Urls @(
    "https://github.com/espeak-ng/espeak-ng/releases/download/$EspeakNgVersion/espeak-ng.msi",
    "https://github.com/espeak-ng/espeak-ng/releases/download/$EspeakNgVersion/espeak-ng-X64.msi",
    "https://github.com/espeak-ng/espeak-ng/releases/download/$EspeakNgVersion/espeak-ng-x64.msi"
)

$modelChecks = @(
    @{
        Name = "Kitten Nano"
        Urls = @(
            "https://huggingface.co/KittenML/kitten-tts-nano-0.8-fp32/resolve/main/config.json",
            "https://huggingface.co/KittenML/kitten-tts-nano-0.8-fp32/resolve/main/kitten_tts_nano_v0_8.onnx",
            "https://huggingface.co/KittenML/kitten-tts-nano-0.8-fp32/resolve/main/voices.npz"
        )
    }
    @{
        Name = "Kitten Nano Int8"
        Urls = @(
            "https://huggingface.co/KittenML/kitten-tts-nano-0.8-int8/resolve/main/config.json",
            "https://huggingface.co/KittenML/kitten-tts-nano-0.8-int8/resolve/main/kitten_tts_nano_v0_8.onnx",
            "https://huggingface.co/KittenML/kitten-tts-nano-0.8-int8/resolve/main/voices.npz"
        )
    }
    @{
        Name = "Kitten Micro"
        Urls = @(
            "https://huggingface.co/KittenML/kitten-tts-micro-0.8/resolve/main/config.json",
            "https://huggingface.co/KittenML/kitten-tts-micro-0.8/resolve/main/kitten_tts_micro_v0_8.onnx",
            "https://huggingface.co/KittenML/kitten-tts-micro-0.8/resolve/main/voices.npz"
        )
    }
    @{
        Name = "Kitten Mini"
        Urls = @(
            "https://huggingface.co/KittenML/kitten-tts-mini-0.8/resolve/main/config.json",
            "https://huggingface.co/KittenML/kitten-tts-mini-0.8/resolve/main/kitten_tts_mini_v0_8.onnx",
            "https://huggingface.co/KittenML/kitten-tts-mini-0.8/resolve/main/voices.npz"
        )
    }
)

foreach ($check in $modelChecks) {
    foreach ($url in $check.Urls) {
        Test-Url -Name $check.Name -Url $url
    }
}

Write-Host "All release source URLs are reachable."
