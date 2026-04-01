# portable_kittentts_cpp

Small portable C++ Kitten TTS project for Windows, built around:

- `kitten-tts-nano-0.8-fp32`, `kitten-tts-nano-0.8-int8`, `kitten-tts-micro-0.8`, and `kitten-tts-mini-0.8` ONNX models
- ONNX Runtime C++ API
- trimmed eSpeak NG runtime for phonemization
- a tiny WAV player using the Windows sound API

## Layout

- `src/` - C++ source
- `data/ipa_symbols.txt` - IPA symbol table used by the tokenizer
- `model/kitten-nano-v0_8-onnx/` - ONNX model plus 8 voice embeddings
- `model/kitten-nano-int8-v0_8-onnx/` - Nano int8 ONNX model plus 8 voice embeddings
- `model/kitten-micro-v0_8-onnx/` - Micro ONNX model plus 8 voice embeddings
- `model/kitten-mini-v0_8-onnx/` - Mini ONNX model plus 8 voice embeddings
- `runtime/espeak-lite/` - trimmed eSpeak NG runtime
- `runtime/onnxruntime/` - local ONNX Runtime DLLs loaded at runtime
- `third_party/onnxruntime/` - headers and import libs for building
- `dist/` - portable runnable bundle produced by `build.ps1`

## First Setup

1. Run `.\bootstrap.ps1`
2. Run `.\build.ps1`

After that, the portable bundle is in `dist\`.

## Run

Preferred Windows launchers:

```powershell
.\nano.bat
.\nano-int8.bat
.\micro.bat
.\mini.bat
```

Interactive mode with the generic wrapper:

```powershell
.\run.ps1
```

One-shot synthesis:

```powershell
.\run.ps1 --speaker male --text "Hello world"
```

List voices:

```powershell
.\run.ps1 --list-speakers
```

`run.bat` and `run.ps1` are still kept as generic compatibility launchers, but the four model-specific `.bat` files are the simplest way to start a model directly.

## Session Mode

Use bare `--session` when you want a single foreground process that keeps one model loaded and reads commands from stdin:

```powershell
.\nano.bat --session --speaker Jasper
speaker=Bella text="Hello world"
speaker=Leo speed=0.9 text="How are you?"
Hello from the default speaker
exit
```

In session mode, each line can set defaults or speak immediately. Common fields are `speaker=...`, `speed=...`, `output=...`, `clean-text=...`, and `text=...`.
Use `noplayback=true` or `playback=false` on a line if you want synthesis without audio output.

For named reusable sessions, use `--session NAME` from your scripts:

```bat
@echo off
kitten_tts.exe --session avatar --model nano --speaker Rosie --text "Hello world"
kitten_tts.exe --session avatar --model nano --speaker Rosie --text "The second line reuses the same session."
kitten_tts.exe --session avatar --terminate
```

The first call auto-launches a reusable background session if needed. Later calls with the same name reuse the already loaded model, and `--terminate` stops that named session cleanly.

## Models

Nano is the default bundled model:

```powershell
.\run.ps1 --model nano --speaker Jasper --text "Hello world"
```

Nano int8 is the smaller variant:

```powershell
.\run.ps1 --model nano-int8 --speaker Jasper --text "Hello world"
```

Micro is also available:

```powershell
.\run.ps1 --model micro --speaker Jasper --text "Hello world"
```

Mini is the largest bundled option:

```powershell
.\run.ps1 --model mini --speaker Jasper --text "Hello world"
```

All model names also accept the longer aliases from earlier builds, such as `kitten-tts-nano-0.8-fp32`, `kitten-tts-nano-0.8-int8`, `kitten-micro`, and `kitten-mini`.

## Kitten voices

The model exposes these voices:

- `Bella` - `expr-voice-2-f`
- `Jasper` - `expr-voice-2-m`
- `Luna` - `expr-voice-3-f`
- `Bruno` - `expr-voice-3-m`
- `Rosie` - `expr-voice-4-f`
- `Hugo` - `expr-voice-4-m`
- `Kiki` - `expr-voice-5-f`
- `Leo` - `expr-voice-5-m`

Examples:

```powershell
.\run.ps1 --speaker Bella --text "Hello world"
.\run.ps1 --speaker Jasper --text "Hello world"
.\run.ps1 --speaker 0 --text "Hello world"
.\run.ps1 --speaker 7 --text "Hello world"
```

`male` maps to `Jasper` and `female` maps to `Bella`.

## Notes

- CPU inference only in this build.
- The portable bundle loads ONNX Runtime from `runtime\onnxruntime\onnxruntime.dll` at runtime, which keeps the EXE portable and the release workflow lightweight.
- `--device` is accepted for compatibility, but this build is CPU-only.
- `--model` selects between the bundled `nano`, `nano-int8`, `micro`, and `mini` models.
- The CLI applies per-voice speed priors when the selected model defines them, so the bundled voices match the reference behavior more closely.
- Raw demo-style phonemization is used by default, matching the Hugging Face Space behavior. Pass `--clean-text` if you want number / currency expansion before phonemizing.
- Output is played from memory by default. Pass `--noplayback` to skip audio playback entirely, or `--output` if you want to keep a WAV file.

## Releases

The GitHub release workflow publishes two kinds of artifacts on `v*` tags:

- `portable_kittentts_cpp-<tag>-dist.zip` - the normal portable `dist/` folder packed as a zip
- `portable_kittentts_cpp-<tag>-nano.exe`, `portable_kittentts_cpp-<tag>-nano-int8.exe`, `portable_kittentts_cpp-<tag>-micro.exe`, `portable_kittentts_cpp-<tag>-mini.exe` - single-file bootstrapper bundles that unpack a model-specific payload to temp and then launch the normal CLI

The single-EXE releases are CLI-compatible: double-clicking opens the interactive prompt, and command-line switches such as `--text`, `--speaker`, `--speed`, `--output`, `--list-speakers`, and the rest of the portable flags are forwarded to the embedded app.
The only model-related difference is that each bundle is fixed to its baked-in model, so the wrapper injects the matching `--model` value for you.
Use the `dist.zip` release if you want all bundled models in one portable folder, or use a single-EXE bundle if you want one model pinned to a double-clickable file.
The wrapper caches the extracted portable app under your user profile after the first real launch, so later runs are much faster.
Pass `--coldstart` to force a fresh temporary extraction and feel the first-launch cost yourself.
`--help` is handled directly by the bootstrapper and does not unpack the payload.
Use `--session` if you want the process to stay alive and accept one command per line, which is handy for scripts or repeated utterances with different speakers.
Push a tag like `v0.1.0` and the GitHub Actions workflow will publish these artifacts automatically.
You can also trigger the same workflow manually from the GitHub Actions tab and enter the release tag there.

## License And Notices

See the repo root `LICENSE` file for the project license.
The repo root [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md) lists the bundled runtime dependencies and model licenses.
When present, `build.ps1` copies both files into `dist/` so release artifacts include the notices.

## Release Checklist

1. Make sure `dist\` builds locally with `.\build.ps1`.
2. Pick a version tag such as `v0.1.0`.
3. Push the tag to GitHub with `git push origin v0.1.0`, or open GitHub Actions and run the `Release` workflow manually with the same tag.
4. Wait for the Actions run to finish.
5. Download either the `dist` zip for the full portable folder, or one of the single-EXE model bundles if you want a one-file launcher.
