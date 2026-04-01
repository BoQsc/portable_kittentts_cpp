#pragma once

#include <filesystem>
#include <cstdlib>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

namespace kit
{
    inline std::string shell_quote(std::string value)
    {
        std::string out = "\"";
        for (char c : value)
        {
            if (c == '"')
            {
                out += "\\\"";
            }
            else
            {
                out.push_back(c);
            }
        }
        out.push_back('"');
        return out;
    }

    inline void play_wav_file(const std::filesystem::path& path)
    {
#ifdef _WIN32
        const std::wstring wide = path.wstring();
        if (!PlaySoundW(wide.c_str(), nullptr, SND_FILENAME | SND_SYNC | SND_NODEFAULT))
        {
            throw std::runtime_error("Failed to play WAV file: " + path.string());
        }
#else
        const std::string quoted = shell_quote(path.string());
        const char* prefixes[] = {
#if defined(__APPLE__)
            "afplay ",
#else
            "aplay ",
            "paplay ",
#endif
            "ffplay -nodisp -autoexit ",
            nullptr,
        };

        for (const char** prefix = prefixes; *prefix; ++prefix)
        {
            const std::string command = std::string(*prefix) + quoted;
            if (std::system(command.c_str()) == 0)
            {
                return;
            }
        }

        throw std::runtime_error("No supported WAV player command was available.");
#endif
    }

#ifdef _WIN32
    inline void play_wav_bytes(const std::vector<std::uint8_t>& wav_bytes)
    {
        if (wav_bytes.empty())
        {
            throw std::runtime_error("Cannot play an empty WAV buffer.");
        }

        if (!PlaySoundW(reinterpret_cast<LPCWSTR>(wav_bytes.data()), nullptr, SND_MEMORY | SND_SYNC | SND_NODEFAULT))
        {
            throw std::runtime_error("Failed to play WAV from memory.");
        }
    }
#endif
}
