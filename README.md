# portable_kittentts_cpp

This repository hosts the release automation and the source project for a small portable Kitten TTS build.

The actual C++ application lives in:

- [portable_kittentts_cpp/](portable_kittentts_cpp/)

Useful entry points:

- [Project README](portable_kittentts_cpp/README.md)
- [Release workflow](.github/workflows/release.yml)

What this repo publishes:

- a normal portable `dist` zip
- per-model single-EXE release bundles for `nano`, `nano-int8`, `micro`, and `mini`

The repo is structured this way so generated runtime assets, models, and release outputs stay out of source control while GitHub Actions builds and publishes them on demand.
