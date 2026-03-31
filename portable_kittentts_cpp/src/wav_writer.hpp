#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace kit
{
    inline void write_u16(std::ofstream& out, std::uint16_t value)
    {
        out.write(reinterpret_cast<const char*>(&value), sizeof(value));
    }

    inline void write_u32(std::ofstream& out, std::uint32_t value)
    {
        out.write(reinterpret_cast<const char*>(&value), sizeof(value));
    }

    inline void write_fourcc(std::ofstream& out, const char* tag)
    {
        out.write(tag, 4);
    }

    inline void write_wav_pcm16(const std::string& path, const std::vector<float>& samples, std::uint32_t sample_rate)
    {
        std::ofstream out(path, std::ios::binary);
        if (!out)
        {
            throw std::runtime_error("Failed to open WAV output: " + path);
        }

        const std::uint32_t data_bytes = static_cast<std::uint32_t>(samples.size() * sizeof(std::int16_t));
        write_fourcc(out, "RIFF");
        write_u32(out, 36 + data_bytes);
        write_fourcc(out, "WAVE");
        write_fourcc(out, "fmt ");
        write_u32(out, 16);
        write_u16(out, 1);
        write_u16(out, 1);
        write_u32(out, sample_rate);
        write_u32(out, sample_rate * sizeof(std::int16_t));
        write_u16(out, sizeof(std::int16_t));
        write_u16(out, 16);
        write_fourcc(out, "data");
        write_u32(out, data_bytes);

        for (float sample : samples)
        {
            float clamped = std::clamp(sample, -1.0f, 1.0f);
            const std::int16_t pcm = static_cast<std::int16_t>(std::lrintf(clamped * 32767.0f));
            write_u16(out, static_cast<std::uint16_t>(pcm));
        }
    }
}
