# portable_kittentts_cpp

This repository hosts the release automation and the source project for a small portable Kitten TTS build.

The actual C++ application lives in:

- [portable_kittentts_cpp/](portable_kittentts_cpp/)

Useful entry points:

- [Project README](portable_kittentts_cpp/README.md)
- [Release workflow](.github/workflows/release.yml)
- [Third-party notices](THIRD_PARTY_NOTICES.md)

What this repo publishes:

- a normal portable `dist` zip
- per-model single-EXE release bundles for `nano`, `nano-int8`, `micro`, and `mini`

The single-EXE bundles are still CLI-compatible. They cache a model-specific payload under your user profile on first launch, set up the portable app root, and forward the normal command-line switches to the embedded `kitten_tts.exe`.
Double-clicking one opens the interactive prompt just like the portable folder build.
The first real launch does the extraction work once, and later runs reuse the cache so startup is much faster.
Pass `--coldstart` if you want a one-off launch that skips the cache and does a fresh temporary extraction instead.
Pass `--session` if you want a persistent command loop that keeps the model loaded and accepts requests from stdin.
Session lines can set `speaker`, `speed`, `output`, and `clean-text`, then speak text without restarting the process.

The repo is structured this way so generated runtime assets, models, and release outputs stay out of source control while GitHub Actions builds and publishes them on demand.

## License And Notices

The repository code is intended to be GPL-3.0-or-later.
The bundled third-party components and model licenses are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
When `LICENSE` is present at the repo root, `build.ps1` copies it into `dist/` alongside the notices file so release artifacts carry both.
