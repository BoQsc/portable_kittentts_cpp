#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace kit
{
    struct VoiceInfo
    {
        std::vector<float> data;
        std::size_t rows = 0;
        std::size_t cols = 0;
    };

    VoiceInfo load_npy_floats(const std::filesystem::path& path);

    std::unordered_map<std::string, VoiceInfo> load_voices(
        const std::filesystem::path& voices_dir,
        const std::vector<std::string>& voice_ids);
}
