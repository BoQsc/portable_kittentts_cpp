#pragma once

#include "utf8.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace kit
{
    class IpaTokenizer
    {
    public:
        explicit IpaTokenizer(const std::filesystem::path& symbols_path)
        {
            if (!std::filesystem::exists(symbols_path))
            {
                throw std::runtime_error("Missing symbol table: " + symbols_path.string());
            }

            const std::u32string symbols =
                U"\u0024\u003b\u003a\u002c\u002e\u0021\u003f\u00a1\u00bf\u2014\u2026\u0022\u00ab\u00bb\u0022\u0022"
                U"\u0020\u0041\u0042\u0043\u0044\u0045\u0046\u0047\u0048\u0049\u004a\u004b\u004c\u004d\u004e\u004f"
                U"\u0050\u0051\u0052\u0053\u0054\u0055\u0056\u0057\u0058\u0059\u005a\u0061\u0062\u0063\u0064\u0065"
                U"\u0066\u0067\u0068\u0069\u006a\u006b\u006c\u006d\u006e\u006f\u0070\u0071\u0072\u0073\u0074\u0075"
                U"\u0076\u0077\u0078\u0079\u007a\u0251\u0250\u0252\u00e6\u0253\u0299\u03b2\u0254\u0255\u00e7\u0257"
                U"\u0256\u00f0\u02a4\u0259\u0258\u025a\u025b\u025c\u025d\u025e\u025f\u0284\u0261\u0260\u0262\u029b"
                U"\u0266\u0267\u0127\u0265\u029c\u0268\u026a\u029d\u026d\u026c\u026b\u026e\u029f\u0271\u026f\u0270\u014b"
                U"\u0273\u0272\u0274\u00f8\u0275\u0278\u03b8\u0153\u0276\u0298\u0279\u027a\u027e\u027b\u0280\u0281"
                U"\u027d\u0282\u0283\u0288\u02a7\u0289\u028a\u028b\u1d1c\u028c\u0263\u0264\u028d\u03c7\u028e\u028f"
                U"\u0291\u0290\u0292\u0294\u02a1\u0295\u02a2\u01c0\u01c1\u01c2\u01c3\u02c8\u02cc\u02d0\u02d1\u02bc"
                U"\u02b4\u02b0\u02b1\u02b2\u02b7\u02e0\u02e4\u02de\u2193\u2191\u2192\u2197\u2198'\u0329'\u1d7b";
            int index = 0;
            for (char32_t cp : symbols)
            {
                _symbol_to_index[cp] = index++;
            }
        }

        std::vector<std::int64_t> tokenize(const std::string& phonemes) const
        {
            std::vector<std::int64_t> tokens;
            tokens.push_back(0);
            for (char32_t cp : utf8_to_u32(phonemes))
            {
                auto it = _symbol_to_index.find(cp);
                if (it != _symbol_to_index.end())
                {
                    tokens.push_back(static_cast<std::int64_t>(it->second));
                }
            }
            tokens.push_back(10);
            tokens.push_back(0);
            return tokens;
        }

    private:
        std::unordered_map<char32_t, int> _symbol_to_index;
    };
}
