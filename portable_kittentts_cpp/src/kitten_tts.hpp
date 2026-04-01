#pragma once

#include "npy_reader.hpp"
#include "tokenizer.hpp"
#include "text_preprocessor.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#define ORT_API_MANUAL_INIT
#include <onnxruntime_cxx_api.h>

namespace kit
{
    struct SpeakerChoice
    {
        std::string alias;
        std::string voice_id;
        float speed_prior = 1.0f;
    };

    struct AudioResult
    {
        std::vector<float> samples;
        std::uint32_t sample_rate = 24000;
        std::string voice_display_name;
    };

    class KittenTtsEngine
    {
    public:
        ~KittenTtsEngine();

        static KittenTtsEngine load(const std::filesystem::path& app_root, const std::string& model_variant);

        void print_speakers() const;
        std::string speakers_text() const;
        AudioResult synthesize(const std::string& text, const std::string& speaker, float speed, bool clean_text = false);

        const std::string& model_name() const { return _model_name; }

    private:
        using EspeakInitializeFn = int (*)(int, int, const char*, int);
        using EspeakTerminateFn = void (*)();
        using EspeakSetVoiceByNameFn = int (*)(const char*);
        using EspeakTextToPhonemesFn = const char* (*)(char**, int, int);

        explicit KittenTtsEngine(
            std::shared_ptr<void> onnxruntime_module,
            std::shared_ptr<void> espeak_module,
            EspeakInitializeFn espeak_initialize,
            EspeakTerminateFn espeak_terminate,
            EspeakSetVoiceByNameFn espeak_set_voice_by_name,
            EspeakTextToPhonemesFn espeak_text_to_phonemes,
            Ort::Env env,
            Ort::Session session,
            IpaTokenizer tokenizer,
            std::unordered_map<std::string, VoiceInfo> voices,
            std::vector<SpeakerChoice> speakers,
            std::unordered_map<std::string, SpeakerChoice> speaker_lookup,
            std::filesystem::path espeak_root,
            std::string model_name);

        SpeakerChoice resolve_speaker(const std::string& speaker) const;
        std::string phonemize_with_espeak(const std::string& text) const;
        std::vector<float> synthesize_chunk(const std::string& chunk_text, const SpeakerChoice& speaker, float speed);

        std::shared_ptr<void> _onnxruntime_module;
        std::shared_ptr<void> _espeak_module;
        EspeakInitializeFn _espeak_initialize{};
        EspeakTerminateFn _espeak_terminate{};
        EspeakSetVoiceByNameFn _espeak_set_voice_by_name{};
        EspeakTextToPhonemesFn _espeak_text_to_phonemes{};
        Ort::Env _env;
        Ort::Session _session;
        IpaTokenizer _tokenizer;
        std::unordered_map<std::string, VoiceInfo> _voices;
        std::vector<SpeakerChoice> _speakers;
        std::unordered_map<std::string, SpeakerChoice> _speaker_lookup;
        std::filesystem::path _espeak_root;
        std::string _model_name;
    };
}
