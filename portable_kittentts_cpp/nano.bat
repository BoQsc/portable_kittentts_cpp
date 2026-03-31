@echo off
setlocal
set "SELF=%~dp0"

if exist "%SELF%kitten_tts.exe" (
  set "KITTEN_TTS_ROOT=%SELF%"
  "%SELF%kitten_tts.exe" --model nano %*
  exit /b %ERRORLEVEL%
)

if exist "%SELF%dist\kitten_tts.exe" (
  set "KITTEN_TTS_ROOT=%SELF%dist"
  "%SELF%dist\kitten_tts.exe" --model nano %*
  exit /b %ERRORLEVEL%
)

if exist "%SELF%build\kitten_tts.exe" (
  set "KITTEN_TTS_ROOT=%SELF%"
  "%SELF%build\kitten_tts.exe" --model nano %*
  exit /b %ERRORLEVEL%
)

echo No executable found. Run build.ps1 first.
exit /b 1
