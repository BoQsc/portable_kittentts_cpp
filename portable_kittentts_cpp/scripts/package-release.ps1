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

function New-SfxPackage {
    Ensure-Directory $artifactRoot
    Reset-Directory $workRoot

    $artifactName = "portable_kittentts_cpp-$Tag-$Model.exe"
    $artifactPath = Join-Path $artifactRoot $artifactName
    $payloadPath = Join-Path $workRoot "payload.zip"
    $launchPath = Join-Path $workRoot "launch.cmd"
    $sedPath = Join-Path $workRoot "package.sed"

    $distItems = Get-DistItems
    if (-not $distItems) {
        throw "dist folder is empty or missing: $distRoot"
    }

    Compress-Archive -Path $distItems -DestinationPath $payloadPath -CompressionLevel Optimal

    @"
@echo off
setlocal
cd /d "%~dp0"
set "KITTEN_TTS_ROOT=%~dp0"
title Kitten TTS $Model
if not exist "kitten_tts.exe" (
  powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -Command "Expand-Archive -LiteralPath '%~dp0payload.zip' -DestinationPath '%~dp0' -Force"
  if errorlevel 1 exit /b %errorlevel%
)
if not exist "kitten_tts.exe" (
  echo Failed to extract kitten_tts.exe.
  exit /b 1
)
kitten_tts.exe --model $Model
set "EXIT_CODE=%errorlevel%"
if not "%EXIT_CODE%"=="0" (
  echo.
  echo Kitten TTS exited with error code %EXIT_CODE%.
  pause
)
exit /b %EXIT_CODE%
"@ | Set-Content -Encoding Ascii -NoNewline $launchPath

    @"
[Version]
Class=IEXPRESS
SEDVersion=3
[Options]
PackagePurpose=InstallApp
ShowInstallProgramWindow=1
HideExtractAnimation=1
UseLongFileName=1
InsideCompressed=0
CAB_FixedSize=0
CAB_ResvCodeSigning=0
RebootMode=N
InstallPrompt=
DisplayLicense=
FinishMessage=
TargetName=$artifactPath
FriendlyName=portable_kittentts_cpp $Tag $Model
AppLaunched=C:\Windows\System32\cmd.exe /d /s /k launch.cmd
PostInstallCmd=<None>
AdminQuietInstCmd=
UserQuietInstCmd=
SourceFiles=SourceFiles
Compress=0
[Strings]
FILE0=payload.zip
FILE1=launch.cmd
[SourceFiles]
SourceFiles0=$workRoot
[SourceFiles0]
%FILE0%=
%FILE1%=
"@ | Set-Content -Encoding Ascii -NoNewline $sedPath

    $iexpress = Get-Command iexpress.exe -ErrorAction SilentlyContinue
    $iexpressPath = if ($iexpress) { $iexpress.Source } else { Join-Path $env:WINDIR "System32\iexpress.exe" }
    if (-not (Test-Path $iexpressPath)) {
        throw "iexpress.exe was not found. It is required to create the single-EXE release artifacts."
    }

    $process = Start-Process -FilePath $iexpressPath -ArgumentList @("/N", $sedPath) -Wait -PassThru
    if ($process.ExitCode -ne 0) {
        throw "iexpress packaging failed with exit code $($process.ExitCode)"
    }

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
    "sfx" { New-SfxPackage }
}
