$ErrorActionPreference = "Stop"

$root = $PSScriptRoot
$dist = Join-Path $root "dist"
$src = Join-Path $root "src"
$ortInclude = Join-Path $root "third_party\onnxruntime\include"

if (-not (Test-Path (Join-Path $root "data\ipa_symbols.txt"))) {
    throw "Run bootstrap.ps1 first so the assets are in place."
}

$zigPath = $null
$zig = Get-Command zig -ErrorAction SilentlyContinue
if ($zig) {
    $zigPath = $zig.Source
}
if (-not $zigPath) {
    $localZig = Join-Path $root "tools\zig\zig.exe"
    if (Test-Path $localZig) {
        $zigPath = $localZig
    }
}
if (-not $zigPath) {
    throw "Zig not found. Install Zig 0.15+ or place zig.exe in tools\zig\zig.exe."
}

Remove-Item $dist -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $dist | Out-Null

$exe = Join-Path $dist "kitten_tts.exe"
$sources = @(
    (Join-Path $src "main.cpp"),
    (Join-Path $src "kitten_tts.cpp"),
    (Join-Path $src "npy_reader.cpp"),
    (Join-Path $src "text_preprocessor.cpp")
)

& $zigPath c++ -std=c++20 -Oz -I $src -I $ortInclude @sources -o $exe -lwinmm -Xlinker /subsystem:console
if ($LASTEXITCODE -ne 0) {
    throw "Build failed."
}

Copy-Item -Force (Join-Path $root "data") (Join-Path $dist "data") -Recurse
Copy-Item -Force (Join-Path $root "model") (Join-Path $dist "model") -Recurse
Copy-Item -Force (Join-Path $root "runtime") (Join-Path $dist "runtime") -Recurse
Copy-Item -Force (Join-Path $root "README.md") (Join-Path $dist "README.md")
Copy-Item -Force (Join-Path $root "run.bat") (Join-Path $dist "run.bat")
Copy-Item -Force (Join-Path $root "run.ps1") (Join-Path $dist "run.ps1")
Copy-Item -Force (Join-Path $root "nano.bat") (Join-Path $dist "nano.bat")
Copy-Item -Force (Join-Path $root "nano-int8.bat") (Join-Path $dist "nano-int8.bat")
Copy-Item -Force (Join-Path $root "micro.bat") (Join-Path $dist "micro.bat")
Copy-Item -Force (Join-Path $root "mini.bat") (Join-Path $dist "mini.bat")

$bytes = (Get-ChildItem -Recurse -File $dist | Measure-Object -Property Length -Sum).Sum
Write-Host ("Built dist at {0} ({1:N2} MB)" -f $dist, ($bytes / 1MB))
