#include "npy_reader.hpp"

#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace kit
{
    static std::uint16_t read_u16(const std::uint8_t* p)
    {
        return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
    }

    static std::uint32_t read_u32(const std::uint8_t* p)
    {
        return static_cast<std::uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
    }

    static std::string read_file_bytes(const std::filesystem::path& path)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
        {
            throw std::runtime_error("Failed to open NPY file: " + path.string());
        }
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

    static std::string parse_between(const std::string& text, const std::string& left, const std::string& right)
    {
        const auto start = text.find(left);
        if (start == std::string::npos)
        {
            return {};
        }
        const auto begin = start + left.size();
        const auto end = text.find(right, begin);
        if (end == std::string::npos || end < begin)
        {
            return {};
        }
        return text.substr(begin, end - begin);
    }

    VoiceInfo load_npy_floats(const std::filesystem::path& path)
    {
        const std::string bytes = read_file_bytes(path);
        if (bytes.size() < 10 || static_cast<unsigned char>(bytes[0]) != 0x93 || bytes.substr(1, 5) != "NUMPY")
        {
            throw std::runtime_error("Not a valid .npy file: " + path.string());
        }

        const std::uint8_t major = static_cast<std::uint8_t>(bytes[6]);
        std::size_t header_len = 0;
        std::size_t header_offset = 0;
        if (major == 1)
        {
            header_len = read_u16(reinterpret_cast<const std::uint8_t*>(bytes.data() + 8));
            header_offset = 10;
        }
        else
        {
            header_len = read_u32(reinterpret_cast<const std::uint8_t*>(bytes.data() + 8));
            header_offset = 12;
        }

        if (header_offset + header_len > bytes.size())
        {
            throw std::runtime_error("Invalid .npy header length: " + path.string());
        }

        const std::string header = bytes.substr(header_offset, header_len);
        const std::string descr = parse_between(header, "'descr': '", "'");
        const std::string shape_text = parse_between(header, "'shape': (", ")");
        if (descr.empty())
        {
            throw std::runtime_error("Could not parse dtype in " + path.string());
        }

        std::vector<std::size_t> shape;
        std::size_t pos = 0;
        while (pos < shape_text.size())
        {
            while (pos < shape_text.size() && (shape_text[pos] == ' ' || shape_text[pos] == ','))
            {
                ++pos;
            }
            const std::size_t start = pos;
            while (pos < shape_text.size() && shape_text[pos] >= '0' && shape_text[pos] <= '9')
            {
                ++pos;
            }
            if (pos > start)
            {
                shape.push_back(static_cast<std::size_t>(std::stoull(shape_text.substr(start, pos - start))));
            }
            else
            {
                ++pos;
            }
        }

        const std::size_t data_offset = header_offset + header_len;
        const std::size_t data_bytes = bytes.size() - data_offset;
        VoiceInfo info;

        if (descr == "<f4" || descr == "|f4" || descr == "float32")
        {
            const std::size_t count = data_bytes / sizeof(float);
            info.data.resize(count);
            std::memcpy(info.data.data(), bytes.data() + data_offset, count * sizeof(float));
        }
        else if (descr == "<f8" || descr == "|f8" || descr == "float64")
        {
            const std::size_t count = data_bytes / sizeof(double);
            info.data.resize(count);
            for (std::size_t i = 0; i < count; ++i)
            {
                double value = 0.0;
                std::memcpy(&value, bytes.data() + data_offset + i * sizeof(double), sizeof(double));
                info.data[i] = static_cast<float>(value);
            }
        }
        else
        {
            throw std::runtime_error("Unsupported dtype in " + path.string() + ": " + descr);
        }

        info.rows = shape.size() >= 1 ? shape[0] : 1;
        info.cols = shape.size() >= 2 ? shape[1] : info.data.size();
        return info;
    }

    std::unordered_map<std::string, VoiceInfo> load_voices(
        const std::filesystem::path& voices_dir,
        const std::vector<std::string>& voice_ids)
    {
        std::unordered_map<std::string, VoiceInfo> voices;
        for (const auto& id : voice_ids)
        {
            const auto path = voices_dir / (id + ".npy");
            voices.emplace(id, load_npy_floats(path));
        }
        return voices;
    }
}
