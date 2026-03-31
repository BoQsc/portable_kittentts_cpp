# Third-Party Notices

This file lists the external components and model assets used by the portable Kitten TTS builds in this repository.
It is a notice file, not a substitute for the upstream license texts.

## Repository code

The source code in this repository is intended to be distributed under GPL-3.0-or-later.
Add the repository root `LICENSE` file with the GPL text when you publish the code.

## Bundled runtime and build dependencies

- [eSpeak NG](https://github.com/espeak-ng/espeak-ng) - GPL-3.0-or-later
  - Used as the phonemization backend.
  - Bundled in the portable releases, so the release artifacts include this GPL component.
- [ONNX Runtime](https://github.com/microsoft/onnxruntime) - MIT
  - Used for model inference in the portable C++ build.
- [Zig](https://ziglang.org/) - MIT
  - Used as the Windows build toolchain in `build.ps1` and GitHub Actions.

## Bundled model assets

The Kitten model files are downloaded from Hugging Face and are covered by their own model licenses:

- [KittenML/kitten-tts-nano-0.8-fp32](https://huggingface.co/KittenML/kitten-tts-nano-0.8-fp32) - Apache-2.0
- [KittenML/kitten-tts-nano-0.8-int8](https://huggingface.co/KittenML/kitten-tts-nano-0.8-int8) - Apache-2.0
- [KittenML/kitten-tts-micro-0.8](https://huggingface.co/KittenML/kitten-tts-micro-0.8) - Apache-2.0
- [KittenML/kitten-tts-mini-0.8](https://huggingface.co/KittenML/kitten-tts-mini-0.8) - Apache-2.0

## Release packaging note

The `dist` folder and the single-EXE release bundles are built from the portable project output.
When the root `LICENSE` and this notice file are present, `build.ps1` copies both into `dist/` so they are included in release artifacts.
