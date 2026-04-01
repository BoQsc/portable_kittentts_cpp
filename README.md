# portable_kittentts_cpp

Portable Kitten TTS for Windows, written in C++.

This repository contains the source project and release automation for a small local text-to-speech build. It ships:

- a normal portable `dist` folder
- one-file release bundles pinned to a specific model
- named reusable sessions for scripting
- offline Windows inference without Python or Node in the release artifacts

For the full CLI reference, model list, and voice examples, see the project README at [portable_kittentts_cpp/README.md](portable_kittentts_cpp/README.md).

## Quick Start

Download the latest GitHub Release and choose the artifact that fits your workflow:

| Artifact | Best for |
| --- | --- |
| `dist.zip` | One portable folder with all bundled models |
| `nano.exe`, `nano-int8.exe`, `micro.exe`, `mini.exe` | One model in one file, with first-launch caching |

From the `dist` folder, the model launchers are:

```powershell
.\nano.bat
.\nano-int8.bat
.\micro.bat
.\mini.bat
```

One-shot synthesis:

```powershell
.\nano.bat --speaker Rosie --text "Hello world"
```

Named reusable session for scripts:

```bat
@echo off
kitten_tts.exe --session avatar --model nano --speaker Rosie --text "Hello world"
kitten_tts.exe --session avatar --model nano --speaker Rosie --text "The second line reuses the same session."
kitten_tts.exe --session avatar --terminate
```

## Speakers

Bundled Kitten voices:

| Name | Voice |
| --- | --- |
| Bella | `expr-voice-2-f` |
| Jasper | `expr-voice-2-m` |
| Luna | `expr-voice-3-f` |
| Bruno | `expr-voice-3-m` |
| Rosie | `expr-voice-4-f` |
| Hugo | `expr-voice-4-m` |
| Kiki | `expr-voice-5-f` |
| Leo | `expr-voice-5-m` |

Aliases:

- `male` maps to `Jasper`
- `female` maps to `Bella`

## What Is Here

- [portable_kittentts_cpp/](portable_kittentts_cpp/) - the actual C++ app and build scripts
- [Release workflow](.github/workflows/release.yml) - builds and publishes releases on GitHub Actions
- [Third-party notices](THIRD_PARTY_NOTICES.md) - bundled runtime and model licenses

## Notes

- The portable app is CPU-only.
- `--session` keeps one model loaded and accepts multiple commands from stdin.
- Single-EXE bundles cache their extracted payload after the first launch.
- `--coldstart` forces a fresh temporary extraction for the single-EXE bundles.
- The root repo stays source-only; generated assets and release outputs are excluded from Git.

## License

The repository code is GPL-3.0-or-later. Third-party component and model licenses are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
