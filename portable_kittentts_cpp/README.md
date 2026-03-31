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
- ONNX Runtime is loaded from `runtime\onnxruntime\onnxruntime.dll` at runtime, so the EXE stays small and does not statically depend on the DLL.
- The portable bundle is pinned to the KittenML Nano 0.8 reference runtime line (`onnxruntime` 1.22.1) so the local output stays closer to the official demo.
- `--device` is accepted for compatibility, but this build is CPU-only.
- `--model` selects between the bundled `nano`, `nano-int8`, `micro`, and `mini` models.
- The CLI applies per-voice speed priors when the selected model defines them, so the bundled voices match the reference behavior more closely.
- Raw demo-style phonemization is used by default, matching the Hugging Face Space behavior. Pass `--clean-text` if you want number / currency expansion before phonemizing.
- Output is written to `infer_outputs\output.wav` under the app root by default and played immediately.
