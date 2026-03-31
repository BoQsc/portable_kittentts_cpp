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
- Output is written to `infer_outputs\output.wav` under the app root by default and played immediately.

## Releases

The GitHub release workflow publishes two kinds of artifacts on `v*` tags:

- `portable_kittentts_cpp-<tag>-dist.zip` - the normal portable `dist/` folder packed as a zip
- `portable_kittentts_cpp-<tag>-nano.exe`, `portable_kittentts_cpp-<tag>-nano-int8.exe`, `portable_kittentts_cpp-<tag>-micro.exe`, `portable_kittentts_cpp-<tag>-mini.exe` - single-file self-extracting bundles that include one model each

The single-EXE releases unpack to a temp folder, expand the bundled payload, and then launch `kitten_tts.exe --model <variant>`.
They are meant as one-click interactive launchers; use the `dist.zip` release if you want the full CLI surface with custom flags.
Push a tag like `v0.1.0` and the GitHub Actions workflow will publish these artifacts automatically.
You can also trigger the same workflow manually from the GitHub Actions tab and enter the release tag there.

## Release Checklist

1. Make sure `dist\` builds locally with `.\build.ps1`.
2. Pick a version tag such as `v0.1.0`.
3. Push the tag to GitHub with `git push origin v0.1.0`, or open GitHub Actions and run the `Release` workflow manually with the same tag.
4. Wait for the Actions run to finish.
5. Download the `dist` zip or the single-EXE model bundles from the GitHub release page.
