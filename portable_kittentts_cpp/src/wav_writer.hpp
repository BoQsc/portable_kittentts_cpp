#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace kit
{
    inline void append_u16(std::vector<std::uint8_t>& out, std::uint16_t value)
    {
        out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
        out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    }

    inline void append_u32(std::vector<std::uint8_t>& out, std::uint32_t value)
    {
        out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
        out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
        out.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
        out.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
    }

    inline void append_fourcc(std::vector<std::uint8_t>& out, const char* tag)
    {
        out.insert(out.end(), tag, tag + 4);
    }

    inline std::vector<std::uint8_t> build_wav_pcm16_bytes(const std::vector<float>& samples, std::uint32_t sample_rate)
    {
        const std::uint32_t data_bytes = static_cast<std::uint32_t>(samples.size() * sizeof(std::int16_t));
        std::vector<std::uint8_t> bytes;
        bytes.reserve(44 + data_bytes);

        append_fourcc(bytes, "RIFF");
        append_u32(bytes, 36 + data_bytes);
        append_fourcc(bytes, "WAVE");
        append_fourcc(bytes, "fmt ");
        append_u32(bytes, 16);
        append_u16(bytes, 1);
        append_u16(bytes, 1);
        append_u32(bytes, sample_rate);
        append_u32(bytes, sample_rate * sizeof(std::int16_t));
        append_u16(bytes, sizeof(std::int16_t));
        append_u16(bytes, 16);
        append_fourcc(bytes, "data");
        append_u32(bytes, data_bytes);

        for (float sample : samples)
        {
            float clamped = std::clamp(sample, -1.0f, 1.0f);
            const std::int16_t pcm = static_cast<std::int16_t>(std::lrintf(clamped * 32767.0f));
            append_u16(bytes, static_cast<std::uint16_t>(pcm));
        }

        return bytes;
    }

    inline void write_wav_pcm16(const std::string& path, const std::vector<float>& samples, std::uint32_t sample_rate)
    {
        std::ofstream out(path, std::ios::binary);
        if (!out)
        {
            throw std::runtime_error("Failed to open WAV output: " + path);
        }

        const std::vector<std::uint8_t> bytes = build_wav_pcm16_bytes(samples, sample_rate);
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
}
