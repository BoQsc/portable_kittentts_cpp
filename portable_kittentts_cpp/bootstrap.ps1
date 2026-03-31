$ErrorActionPreference = "Stop"

$root = $PSScriptRoot
$playground = Split-Path $root -Parent
$nativeRoot = Join-Path $playground "portable_kittentts_native"
$nugetRoot = Join-Path $env:USERPROFILE ".nuget\packages\microsoft.ml.onnxruntime\1.24.4"

if (-not (Test-Path $nativeRoot)) {
    throw "Expected native asset source folder not found: $nativeRoot"
}
if (-not (Test-Path $nugetRoot)) {
    throw "Expected ONNX Runtime NuGet cache not found: $nugetRoot"
}

New-Item -ItemType Directory -Force -Path (Join-Path $root "data") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $root "model\kitten-nano-v0_8-onnx\onnx") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $root "model\kitten-nano-v0_8-onnx\voices") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $root "runtime\espeak-lite") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $root "runtime\onnxruntime") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $root "third_party\onnxruntime\include") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $root "third_party\onnxruntime\lib") | Out-Null

Copy-Item -Force (Join-Path $nativeRoot "data\ipa_symbols.txt") (Join-Path $root "data\ipa_symbols.txt")
Copy-Item -Force (Join-Path $nativeRoot "model\kitten-nano-v0_8-onnx\config.json") (Join-Path $root "model\kitten-nano-v0_8-onnx\config.json")
Copy-Item -Force (Join-Path $nativeRoot "model\kitten-nano-v0_8-onnx\kitten_config.json") (Join-Path $root "model\kitten-nano-v0_8-onnx\kitten_config.json")
Copy-Item -Force (Join-Path $nativeRoot "model\kitten-nano-v0_8-onnx\onnx\model.onnx") (Join-Path $root "model\kitten-nano-v0_8-onnx\onnx\model.onnx")

$voicesSrc = Join-Path $nativeRoot "model\kitten-nano-v0_8-onnx\voices.npz"
$voicesDst = Join-Path $root "model\kitten-nano-v0_8-onnx\voices"
@'
import pathlib
import shutil
import sys
import zipfile

src = pathlib.Path(sys.argv[1])
dst = pathlib.Path(sys.argv[2])
dst.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(src) as zf:
    for info in zf.infolist():
        target = dst / info.filename
        with zf.open(info) as handle, open(target, "wb") as out:
            shutil.copyfileobj(handle, out)
'@ | python - $voicesSrc $voicesDst

$nanoInt8Root = Join-Path $root "model\kitten-nano-int8-v0_8-onnx"
$nanoInt8Config = Join-Path $nanoInt8Root "config.json"
$nanoInt8KittenConfig = Join-Path $nanoInt8Root "kitten_config.json"
$nanoInt8Model = Join-Path $nanoInt8Root "onnx\model.onnx"
$nanoInt8VoicesDir = Join-Path $nanoInt8Root "voices"
if (-not (Test-Path $nanoInt8Config) -or -not (Test-Path $nanoInt8Model) -or -not (Test-Path $nanoInt8VoicesDir)) {
    New-Item -ItemType Directory -Force -Path (Join-Path $nanoInt8Root "onnx") | Out-Null
    New-Item -ItemType Directory -Force -Path $nanoInt8VoicesDir | Out-Null

    $nanoInt8ConfigUrl = "https://huggingface.co/KittenML/kitten-tts-nano-0.8-int8/resolve/main/config.json"
    $nanoInt8ModelUrl = "https://huggingface.co/KittenML/kitten-tts-nano-0.8-int8/resolve/main/kitten_tts_nano_v0_8.onnx"
    $nanoInt8VoicesUrl = "https://huggingface.co/KittenML/kitten-tts-nano-0.8-int8/resolve/main/voices.npz"
    $nanoInt8VoicesNpz = Join-Path $env:TEMP "kitten-nano-int8-voices.npz"

    Invoke-WebRequest -Uri $nanoInt8ConfigUrl -OutFile $nanoInt8Config
    Copy-Item -Force $nanoInt8Config $nanoInt8KittenConfig
    Invoke-WebRequest -Uri $nanoInt8ModelUrl -OutFile $nanoInt8Model
    Invoke-WebRequest -Uri $nanoInt8VoicesUrl -OutFile $nanoInt8VoicesNpz

    @'
import pathlib
import shutil
import sys
import zipfile

src = pathlib.Path(sys.argv[1])
dst = pathlib.Path(sys.argv[2])
dst.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(src) as zf:
    for info in zf.infolist():
        target = dst / info.filename
        with zf.open(info) as handle, open(target, "wb") as out:
            shutil.copyfileobj(handle, out)
'@ | python - $nanoInt8VoicesNpz $nanoInt8VoicesDir

    Remove-Item $nanoInt8VoicesNpz -Force
} else {
    Write-Host "Nano int8 model already present; skipping download."
}

$microConfigSrc = Join-Path $playground "_micro_config.json"
$microModelSrc = Join-Path $playground "_micro_kitten_tts_micro_v0_8.onnx"
$microVoicesSrc = Join-Path $playground "_micro_voices.npz"
$microRoot = Join-Path $root "model\kitten-micro-v0_8-onnx"
if ((Test-Path $microConfigSrc) -and (Test-Path $microModelSrc) -and (Test-Path $microVoicesSrc)) {
    New-Item -ItemType Directory -Force -Path (Join-Path $microRoot "onnx") | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $microRoot "voices") | Out-Null

    Copy-Item -Force $microConfigSrc (Join-Path $microRoot "config.json")
    Copy-Item -Force $microConfigSrc (Join-Path $microRoot "kitten_config.json")
    Copy-Item -Force $microModelSrc (Join-Path $microRoot "onnx\model.onnx")

    $microVoicesDst = Join-Path $microRoot "voices"
    @'
import pathlib
import shutil
import sys
import zipfile

src = pathlib.Path(sys.argv[1])
dst = pathlib.Path(sys.argv[2])
dst.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(src) as zf:
    for info in zf.infolist():
        target = dst / info.filename
        with zf.open(info) as handle, open(target, "wb") as out:
            shutil.copyfileobj(handle, out)
'@ | python - $microVoicesSrc $microVoicesDst
} else {
    Write-Warning "Micro assets not found at workspace root; skipping micro model bootstrap."
}

$miniRoot = Join-Path $root "model\kitten-mini-v0_8-onnx"
$miniConfig = Join-Path $miniRoot "config.json"
$miniKittenConfig = Join-Path $miniRoot "kitten_config.json"
$miniModel = Join-Path $miniRoot "onnx\model.onnx"
$miniVoicesDir = Join-Path $miniRoot "voices"
if (-not (Test-Path $miniConfig) -or -not (Test-Path $miniModel) -or -not (Test-Path $miniVoicesDir)) {
    New-Item -ItemType Directory -Force -Path (Join-Path $miniRoot "onnx") | Out-Null
    New-Item -ItemType Directory -Force -Path $miniVoicesDir | Out-Null

    $miniConfigUrl = "https://huggingface.co/KittenML/kitten-tts-mini-0.8/resolve/main/config.json"
    $miniModelUrl = "https://huggingface.co/KittenML/kitten-tts-mini-0.8/resolve/main/kitten_tts_mini_v0_8.onnx"
    $miniVoicesUrl = "https://huggingface.co/KittenML/kitten-tts-mini-0.8/resolve/main/voices.npz"
    $miniVoicesNpz = Join-Path $env:TEMP "kitten-mini-voices.npz"

    Invoke-WebRequest -Uri $miniConfigUrl -OutFile $miniConfig
    Copy-Item -Force $miniConfig $miniKittenConfig
    Invoke-WebRequest -Uri $miniModelUrl -OutFile $miniModel
    Invoke-WebRequest -Uri $miniVoicesUrl -OutFile $miniVoicesNpz

    @'
import pathlib
import shutil
import sys
import zipfile

src = pathlib.Path(sys.argv[1])
dst = pathlib.Path(sys.argv[2])
dst.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(src) as zf:
    for info in zf.infolist():
        target = dst / info.filename
        with zf.open(info) as handle, open(target, "wb") as out:
            shutil.copyfileobj(handle, out)
'@ | python - $miniVoicesNpz $miniVoicesDir

    Remove-Item $miniVoicesNpz -Force
} else {
    Write-Host "Mini model already present; skipping download."
}

Copy-Item -Force (Join-Path $nativeRoot "third_party\espeak-lite\*") (Join-Path $root "runtime\espeak-lite") -Recurse

Copy-Item -Force (Join-Path $nugetRoot "runtimes\win-x64\native\onnxruntime.dll") (Join-Path $root "runtime\onnxruntime\onnxruntime.dll")
Copy-Item -Force (Join-Path $nugetRoot "runtimes\win-x64\native\onnxruntime_providers_shared.dll") (Join-Path $root "runtime\onnxruntime\onnxruntime_providers_shared.dll")

Copy-Item -Force (Join-Path $nugetRoot "build\native\include\*") (Join-Path $root "third_party\onnxruntime\include") -Recurse
Copy-Item -Force (Join-Path $nugetRoot "runtimes\win-x64\native\onnxruntime.lib") (Join-Path $root "third_party\onnxruntime\lib\onnxruntime.lib")
Copy-Item -Force (Join-Path $nugetRoot "runtimes\win-x64\native\onnxruntime_providers_shared.lib") (Join-Path $root "third_party\onnxruntime\lib\onnxruntime_providers_shared.lib")

Write-Host "Bootstrap complete."
