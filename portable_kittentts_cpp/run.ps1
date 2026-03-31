$ErrorActionPreference = "Stop"

$root = $PSScriptRoot
$selfExe = Join-Path $root "kitten_tts.exe"
$distExe = Join-Path $root "dist\kitten_tts.exe"
$buildExe = Join-Path $root "build\kitten_tts.exe"

if (Test-Path $selfExe) {
    $exe = $selfExe
    $rootForAssets = $root
}
elseif (Test-Path $distExe) {
    $exe = $distExe
    $rootForAssets = Join-Path $root "dist"
}
elseif (Test-Path $buildExe) {
    $exe = $buildExe
    $rootForAssets = $root
}
else {
    throw "No executable found. Run build.ps1 first."
}

$env:KITTEN_TTS_ROOT = $rootForAssets

& $exe @args
