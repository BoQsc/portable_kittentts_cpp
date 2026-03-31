#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kit
{
    std::string preprocess_text(std::string text);
    std::vector<std::string> chunk_text(const std::string& text, std::size_t max_len = 400);
    std::vector<std::pair<bool, std::string>> split_punctuation_sections(const std::string& text);
    std::string to_lower_ascii(std::string text);
}
