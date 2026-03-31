#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace kit
{
    inline bool is_ascii_alpha(char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }

    inline bool is_ascii_digit(char c)
    {
        return c >= '0' && c <= '9';
    }

    inline bool is_ascii_alnum(char c)
    {
        return is_ascii_alpha(c) || is_ascii_digit(c);
    }

    inline char ascii_lower(char c)
    {
        if (c >= 'A' && c <= 'Z')
        {
            return static_cast<char>(c - 'A' + 'a');
        }
        return c;
    }

    inline std::u32string utf8_to_u32(std::string_view s)
    {
        std::u32string out;
        for (std::size_t i = 0; i < s.size();)
        {
            unsigned char c = static_cast<unsigned char>(s[i]);
            char32_t cp = 0;
            std::size_t extra = 0;
            if (c < 0x80)
            {
                cp = c;
                extra = 0;
            }
            else if ((c >> 5) == 0x6 && i + 1 < s.size())
            {
                cp = c & 0x1F;
                extra = 1;
            }
            else if ((c >> 4) == 0xE && i + 2 < s.size())
            {
                cp = c & 0x0F;
                extra = 2;
            }
            else if ((c >> 3) == 0x1E && i + 3 < s.size())
            {
                cp = c & 0x07;
                extra = 3;
            }
            else
            {
                ++i;
                continue;
            }

            bool valid = true;
            for (std::size_t j = 1; j <= extra; ++j)
            {
                unsigned char cc = static_cast<unsigned char>(s[i + j]);
                if ((cc >> 6) != 0x2)
                {
                    valid = false;
                    break;
                }
                cp = (cp << 6) | (cc & 0x3F);
            }

            if (valid)
            {
                out.push_back(cp);
                i += extra + 1;
            }
            else
            {
                ++i;
            }
        }
        return out;
    }

    inline std::string u32_to_utf8(std::u32string_view s)
    {
        std::string out;
        for (char32_t cp : s)
        {
            if (cp <= 0x7F)
            {
                out.push_back(static_cast<char>(cp));
            }
            else if (cp <= 0x7FF)
            {
                out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            }
            else if (cp <= 0xFFFF)
            {
                out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
                out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            }
            else
            {
                out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
                out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            }
        }
        return out;
    }
}
