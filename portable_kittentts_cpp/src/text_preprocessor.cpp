#include "utf8.hpp"
#include "text_preprocessor.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <regex>
#include <sstream>

namespace kit
{
    namespace
    {
        const std::regex kReUrl(R"(https?:\/\/\S+|www\.\S+)", std::regex::ECMAScript | std::regex::icase);
        const std::regex kReEmail(R"(\b[\w.+-]+@[\w-]+\.[a-z]{2,}\b)", std::regex::ECMAScript | std::regex::icase);
        const std::regex kReHtml(R"(<[^>]+>)", std::regex::ECMAScript);

        const std::regex kReOrdinal(R"((\d+)(st|nd|rd|th)\b)", std::regex::ECMAScript | std::regex::icase);
        const std::regex kRePercent(R"((-?[\d,]+(?:\.\d+)?)\s*%)", std::regex::ECMAScript);
        const std::regex kReCurrency(R"(\$\s*([\d,]+(?:\.\d+)?)\s*([KMBT])?(?![a-zA-Z\d]))", std::regex::ECMAScript);
        const std::regex kReTime(R"((\d{1,2}):(\d{2})(?::(\d{2}))?\s*(am|pm)?\b)", std::regex::ECMAScript | std::regex::icase);
        const std::regex kReRange(R"((\d+)-(\d+))", std::regex::ECMAScript);
        const std::regex kReModelVer(R"(\b([a-zA-Z][a-zA-Z0-9]*)-(\d[\d.]*)(?=[^\d.]|$))", std::regex::ECMAScript);
        const std::regex kReUnit(R"((\d+(?:\.\d+)?)\s*(km|kg|mg|ml|gb|mb|kb|tb|hz|khz|mhz|ghz|mph|kph|ms|ns|us)\b)", std::regex::ECMAScript | std::regex::icase);
        const std::regex kReScale(R"((\d+(?:\.\d+)?)\s*([KMBT])\b)", std::regex::ECMAScript);
        const std::regex kReSci(R"((-?\d+(?:\.\d+)?)[eE]([+-]?\d+))", std::regex::ECMAScript);
        const std::regex kReFraction(R"((\d+)\s*/\s*(\d+))", std::regex::ECMAScript);
        const std::regex kReDecade(R"((\d{1,3})0s\b)", std::regex::ECMAScript);
        const std::regex kReNumber(R"(-?[\d,]+(?:\.\d+)?)", std::regex::ECMAScript);
        const std::regex kReSpaces(R"(\s+)", std::regex::ECMAScript);

        bool is_word_char(char c)
        {
            return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
        }

        bool should_replace_number(const std::string& text, std::size_t pos, std::size_t len)
        {
            const bool prev_ok = pos == 0 || !std::isalpha(static_cast<unsigned char>(text[pos - 1]));
            const bool next_ok = pos + len >= text.size() || !std::isalpha(static_cast<unsigned char>(text[pos + len]));
            return prev_ok && next_ok;
        }

        template <typename Fn, typename Pred>
        std::string replace_regex(const std::string& input, const std::regex& re, Fn fn, Pred pred)
        {
            std::string output;
            std::size_t last = 0;

            for (std::sregex_iterator it(input.begin(), input.end(), re), end; it != end; ++it)
            {
                const auto& match = *it;
                const std::size_t pos = static_cast<std::size_t>(match.position());
                const std::size_t len = static_cast<std::size_t>(match.length());
                output.append(input, last, pos - last);
                if (pred(input, pos, len, match))
                {
                    output += fn(match, input, pos, len);
                }
                else
                {
                    output.append(input, pos, len);
                }
                last = pos + len;
            }

            output.append(input, last, std::string::npos);
            return output;
        }

        std::string three_digits_to_words(int n)
        {
            static const char* ones[] = {
                "", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine",
                "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen",
                "seventeen", "eighteen", "nineteen",
            };
            static const char* tens[] = {
                "", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"
            };

            if (n == 0)
            {
                return {};
            }

            std::vector<std::string> parts;
            const int hundreds = n / 100;
            const int rem = n % 100;
            if (hundreds)
            {
                parts.emplace_back(std::string(ones[hundreds]) + " hundred");
            }

            if (rem < 20)
            {
                if (rem)
                {
                    parts.emplace_back(ones[rem]);
                }
            }
            else
            {
                const char* tens_word = tens[rem / 10];
                const char* ones_word = ones[rem % 10];
                if (ones_word[0] != '\0')
                {
                    parts.emplace_back(std::string(tens_word) + "-" + ones_word);
                }
                else
                {
                    parts.emplace_back(tens_word);
                }
            }

            std::ostringstream out;
            for (std::size_t i = 0; i < parts.size(); ++i)
            {
                if (i)
                {
                    out << ' ';
                }
                out << parts[i];
            }
            return out.str();
        }

        std::string number_to_words(long long value)
        {
            static const char* scale[] = {"", "thousand", "million", "billion", "trillion"};
            if (value == 0)
            {
                return "zero";
            }
            if (value < 0)
            {
                return "negative " + number_to_words(-value);
            }

            std::vector<std::string> parts;
            long long remaining = value;
            for (int i = 0; i < 5; ++i)
            {
                const int chunk = static_cast<int>(remaining % 1000);
                if (chunk)
                {
                    const std::string words = three_digits_to_words(chunk);
                    if (scale[i][0] != '\0')
                    {
                        parts.emplace_back(words + " " + scale[i]);
                    }
                    else
                    {
                        parts.emplace_back(words);
                    }
                }
                remaining /= 1000;
                if (remaining == 0)
                {
                    break;
                }
            }

            std::reverse(parts.begin(), parts.end());
            std::ostringstream out;
            for (std::size_t i = 0; i < parts.size(); ++i)
            {
                if (i)
                {
                    out << ' ';
                }
                out << parts[i];
            }
            return out.str();
        }

        std::string float_to_words(const std::string& value)
        {
            const bool negative = !value.empty() && value[0] == '-';
            const std::string clean = negative ? value.substr(1) : value;
            std::string result;
            const auto dot = clean.find('.');
            if (dot != std::string::npos)
            {
                const std::string int_part = dot == 0 ? "0" : clean.substr(0, dot);
                const std::string dec_part = clean.substr(dot + 1);
                std::ostringstream out;
                out << number_to_words(std::stoll(int_part)) << " point ";
                for (std::size_t i = 0; i < dec_part.size(); ++i)
                {
                    if (i)
                    {
                        out << ' ';
                    }
                    out << number_to_words(dec_part[i] - '0');
                }
                result = out.str();
            }
            else
            {
                result = number_to_words(std::stoll(clean));
            }
            return negative ? "negative " + result : result;
        }

        std::string ordinal_suffix(int value)
        {
            const std::string word = number_to_words(value);
            std::string prefix;
            std::string last;
            std::string joiner;

            const auto hyphen = word.find_last_of('-');
            if (hyphen != std::string::npos)
            {
                prefix = word.substr(0, hyphen);
                last = word.substr(hyphen + 1);
                joiner = "-";
            }
            else
            {
                const auto space = word.find_last_of(' ');
                if (space != std::string::npos)
                {
                    prefix = word.substr(0, space);
                    last = word.substr(space + 1);
                    joiner = " ";
                }
                else
                {
                    last = word;
                }
            }

            if (last == "one") last = "first";
            else if (last == "two") last = "second";
            else if (last == "three") last = "third";
            else if (last == "four") last = "fourth";
            else if (last == "five") last = "fifth";
            else if (last == "six") last = "sixth";
            else if (last == "seven") last = "seventh";
            else if (last == "eight") last = "eighth";
            else if (last == "nine") last = "ninth";
            else if (last == "twelve") last = "twelfth";
            else if (!last.empty() && last.back() == 't') last += "h";
            else if (!last.empty() && last.back() == 'e') last = last.substr(0, last.size() - 1) + "th";
            else last += "th";

            return prefix.empty() ? last : prefix + joiner + last;
        }

        std::string expand_ordinals(const std::string& input)
        {
            return replace_regex(
                input,
                kReOrdinal,
                [](const std::smatch& m, const std::string&, std::size_t, std::size_t)
                {
                    return ordinal_suffix(std::stoi(m.str(1)));
                },
                [](const std::string&, std::size_t, std::size_t, const std::smatch&)
                {
                    return true;
                });
        }

        std::string expand_percentages(const std::string& input)
        {
            return replace_regex(
                input,
                kRePercent,
                [](const std::smatch& m, const std::string&, std::size_t, std::size_t)
                {
                    const std::string raw = m.str(1);
                    return raw.find('.') != std::string::npos ? float_to_words(raw) : number_to_words(std::stoll(raw));
                },
                [](const std::string&, std::size_t, std::size_t, const std::smatch&)
                {
                    return true;
                });
        }

        std::string expand_currency(const std::string& input)
        {
            return replace_regex(
                input,
                kReCurrency,
                [](const std::smatch& m, const std::string&, std::size_t, std::size_t)
                {
                    const std::string raw = m.str(1);
                    const std::string scale = m.str(2);
                    const std::string num = raw.find('.') != std::string::npos ? float_to_words(raw) : number_to_words(std::stoll(raw));
                    if (!scale.empty())
                    {
                        const std::string scale_word = scale == "K" || scale == "k" ? "thousand"
                            : scale == "M" || scale == "m" ? "million"
                            : scale == "B" || scale == "b" ? "billion"
                            : "trillion";
                        return num + " " + scale_word + " dollars";
                    }
                    return num + " dollars";
                },
                [](const std::string&, std::size_t, std::size_t, const std::smatch&)
                {
                    return true;
                });
        }

        std::string expand_time(const std::string& input)
        {
            return replace_regex(
                input,
                kReTime,
                [](const std::smatch& m, const std::string&, std::size_t, std::size_t)
                {
                    const int hour = std::stoi(m.str(1));
                    const int minute = std::stoi(m.str(2));
                    const std::string suffix = m.str(4);
                    const std::string h_words = number_to_words(hour);
                    const std::string sfx = suffix.empty() ? "" : " " + to_lower_ascii(suffix);
                    if (minute == 0)
                    {
                        return suffix.empty() ? h_words + " hundred" : h_words + sfx;
                    }
                    if (minute < 10)
                    {
                        return h_words + " oh " + number_to_words(minute) + sfx;
                    }
                    return h_words + " " + number_to_words(minute) + sfx;
                },
                [](const std::string&, std::size_t, std::size_t, const std::smatch&)
                {
                    return true;
                });
        }

        std::string expand_ranges(const std::string& input)
        {
            return replace_regex(
                input,
                kReRange,
                [](const std::smatch& m, const std::string&, std::size_t, std::size_t)
                {
                    return number_to_words(std::stoll(m.str(1))) + " to " + number_to_words(std::stoll(m.str(2)));
                },
                [](const std::string&, std::size_t, std::size_t, const std::smatch&)
                {
                    return true;
                });
        }

        std::string expand_model_names(const std::string& input)
        {
            return replace_regex(
                input,
                kReModelVer,
                [](const std::smatch& m, const std::string&, std::size_t, std::size_t)
                {
                    return m.str(1) + " " + m.str(2);
                },
                [](const std::string&, std::size_t, std::size_t, const std::smatch&)
                {
                    return true;
                });
        }

        std::string expand_units(const std::string& input)
        {
            return replace_regex(
                input,
                kReUnit,
                [](const std::smatch& m, const std::string&, std::size_t, std::size_t)
                {
                    const std::string raw = m.str(1);
                    const std::string unit = to_lower_ascii(m.str(2));
                    const std::string num = raw.find('.') != std::string::npos ? float_to_words(raw) : number_to_words(std::stoll(raw));
                    const std::string expanded =
                        unit == "km" ? "kilometers" :
                        unit == "kg" ? "kilograms" :
                        unit == "mg" ? "milligrams" :
                        unit == "ml" ? "milliliters" :
                        unit == "gb" ? "gigabytes" :
                        unit == "mb" ? "megabytes" :
                        unit == "kb" ? "kilobytes" :
                        unit == "tb" ? "terabytes" :
                        unit == "hz" ? "hertz" :
                        unit == "khz" ? "kilohertz" :
                        unit == "mhz" ? "megahertz" :
                        unit == "ghz" ? "gigahertz" :
                        unit == "mph" ? "miles per hour" :
                        unit == "kph" ? "kilometers per hour" :
                        unit == "ms" ? "milliseconds" :
                        unit == "ns" ? "nanoseconds" :
                        unit == "us" ? "microseconds" : unit;
                    return num + " " + expanded;
                },
                [](const std::string&, std::size_t, std::size_t, const std::smatch&)
                {
                    return true;
                });
        }

        std::string expand_scale_suffixes(const std::string& input)
        {
            return replace_regex(
                input,
                kReScale,
                [](const std::smatch& m, const std::string&, std::size_t, std::size_t)
                {
                    const std::string raw = m.str(1);
                    const std::string scale = m.str(2);
                    const std::string num = raw.find('.') != std::string::npos ? float_to_words(raw) : number_to_words(std::stoll(raw));
                    const std::string scale_word =
                        scale == "K" ? "thousand" :
                        scale == "M" ? "million" :
                        scale == "B" ? "billion" :
                        "trillion";
                    return num + " " + scale_word;
                },
                [](const std::string&, std::size_t, std::size_t, const std::smatch&)
                {
                    return true;
                });
        }

        std::string expand_scientific(const std::string& input)
        {
            return replace_regex(
                input,
                kReSci,
                [](const std::smatch& m, const std::string&, std::size_t, std::size_t)
                {
                    const std::string coeff = m.str(1);
                    const int exp = std::stoi(m.str(2));
                    const std::string coeff_words = coeff.find('.') != std::string::npos ? float_to_words(coeff) : number_to_words(std::stoll(coeff));
                    const std::string sign = exp < 0 ? "negative " : "";
                    return coeff_words + " times ten to the " + sign + number_to_words(std::llabs(exp));
                },
                [](const std::string&, std::size_t, std::size_t, const std::smatch&)
                {
                    return true;
                });
        }

        std::string expand_fractions(const std::string& input)
        {
            return replace_regex(
                input,
                kReFraction,
                [](const std::smatch& m, const std::string&, std::size_t, std::size_t)
                {
                    const int num = std::stoi(m.str(1));
                    const int den = std::stoi(m.str(2));
                    if (den == 0)
                    {
                        return m.str(0);
                    }
                    std::string dword;
                    if (den == 2)
                    {
                        dword = num == 1 ? "half" : "halves";
                    }
                    else if (den == 4)
                    {
                        dword = num == 1 ? "quarter" : "quarters";
                    }
                    else
                    {
                        dword = ordinal_suffix(den);
                        if (num != 1)
                        {
                            dword += 's';
                        }
                    }
                    return number_to_words(num) + " " + dword;
                },
                [](const std::string&, std::size_t, std::size_t, const std::smatch&)
                {
                    return true;
                });
        }

        std::string expand_decades(const std::string& input)
        {
            return replace_regex(
                input,
                kReDecade,
                [](const std::smatch& m, const std::string&, std::size_t, std::size_t)
                {
                    const int base = std::stoi(m.str(1));
                    if (base < 10)
                    {
                        static const char* decade_words[] = {
                            "hundreds", "tens", "twenties", "thirties", "forties",
                            "fifties", "sixties", "seventies", "eighties", "nineties"
                        };
                        return std::string(decade_words[base % 10]);
                    }
                    static const char* decade_words[] = {
                        "hundreds", "tens", "twenties", "thirties", "forties",
                        "fifties", "sixties", "seventies", "eighties", "nineties"
                    };
                    return number_to_words(base / 10) + " " + decade_words[base % 10];
                },
                [](const std::string&, std::size_t, std::size_t, const std::smatch&)
                {
                    return true;
                });
        }

        std::string replace_numbers(const std::string& input)
        {
            return replace_regex(
                input,
                kReNumber,
                [](const std::smatch& m, const std::string&, std::size_t, std::size_t)
                {
                    const std::string clean = m.str(0);
                    return clean.find('.') != std::string::npos ? float_to_words(clean) : number_to_words(std::stoll(clean));
                },
                [](const std::string& text, std::size_t pos, std::size_t len, const std::smatch&)
                {
                    return should_replace_number(text, pos, len);
                });
        }

        std::string normalize_leading_decimals(const std::string& input)
        {
            std::string out;
            out.reserve(input.size() + 8);
            for (std::size_t i = 0; i < input.size(); ++i)
            {
                if (input[i] == '.' && i > 0 && input[i - 1] == '-' && i + 1 < input.size() && std::isdigit(static_cast<unsigned char>(input[i + 1])))
                {
                    out.back() = '-';
                    out.push_back('0');
                    out.push_back('.');
                    continue;
                }
                if (input[i] == '.' && (i == 0 || !std::isdigit(static_cast<unsigned char>(input[i - 1]))) && i + 1 < input.size() && std::isdigit(static_cast<unsigned char>(input[i + 1])))
                {
                    out.push_back('0');
                    out.push_back('.');
                    continue;
                }
                out.push_back(input[i]);
            }
            return out;
        }

        std::string remove_non_prosodic_punct(const std::string& input)
        {
            std::string out;
            out.reserve(input.size());
            for (unsigned char ch : input)
            {
                if (std::isalnum(ch) || std::isspace(ch) || ch == '.' || ch == ',' || ch == '?' || ch == '!' || ch == ';' || ch == ':' || ch == '-' || ch == '\'' )
                {
                    out.push_back(static_cast<char>(ch));
                }
                else
                {
                    out.push_back(' ');
                }
            }
            return out;
        }
    }

    std::string to_lower_ascii(std::string text)
    {
        for (char& c : text)
        {
            c = ascii_lower(c);
        }
        return text;
    }

    std::string preprocess_text(std::string text)
    {
        text = std::regex_replace(text, kReUrl, "");
        text = std::regex_replace(text, kReEmail, "");
        text = std::regex_replace(text, kReHtml, " ");
        text = normalize_leading_decimals(text);

        text = expand_currency(text);
        text = expand_percentages(text);
        text = expand_scientific(text);
        text = expand_time(text);
        text = expand_ordinals(text);
        text = expand_units(text);
        text = expand_scale_suffixes(text);
        text = expand_fractions(text);
        text = expand_decades(text);
        text = expand_ranges(text);
        text = expand_model_names(text);
        text = replace_numbers(text);

        text = remove_non_prosodic_punct(text);
        text = to_lower_ascii(text);
        text = std::regex_replace(text, kReSpaces, " ");
        while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
        {
            text.erase(text.begin());
        }
        while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
        {
            text.pop_back();
        }
        return text;
    }

    std::vector<std::pair<bool, std::string>> split_punctuation_sections(const std::string& text)
    {
        std::vector<std::pair<bool, std::string>> sections;
        std::size_t last = 0;
        auto is_punct = [](char c)
        {
            switch (c)
            {
            case ';': case ':': case '.': case ',': case '?': case '!': case '-': case '\'': case '"':
            case '(': case ')': case '[': case ']': case '{': case '}':
                return true;
            default:
                return false;
            }
        };

        for (std::size_t i = 0; i < text.size();)
        {
            if (std::isspace(static_cast<unsigned char>(text[i])) || is_punct(text[i]))
            {
                std::size_t j = i;
                while (j < text.size() && (std::isspace(static_cast<unsigned char>(text[j])) || is_punct(text[j])))
                {
                    ++j;
                }
                if (i > last)
                {
                    sections.emplace_back(false, text.substr(last, i - last));
                }
                sections.emplace_back(true, text.substr(i, j - i));
                last = j;
                i = j;
            }
            else
            {
                ++i;
            }
        }

        if (last < text.size())
        {
            sections.emplace_back(false, text.substr(last));
        }
        return sections;
    }

    std::vector<std::string> chunk_text(const std::string& text, std::size_t max_len)
    {
        std::vector<std::string> chunks;
        std::string sentence;

        auto trim = [](std::string& value)
        {
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
            {
                value.erase(value.begin());
            }
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
            {
                value.pop_back();
            }
        };

        auto flush_sentence = [&](std::string piece)
        {
            trim(piece);
            if (piece.empty())
            {
                return;
            }

            if (piece.size() <= max_len)
            {
                if (piece.back() != '.' && piece.back() != '!' && piece.back() != '?' && piece.back() != ',' && piece.back() != ';' && piece.back() != ':')
                {
                    piece.push_back(',');
                }
                chunks.push_back(std::move(piece));
                return;
            }

            std::istringstream words(piece);
            std::string current;
            std::string word;
            while (words >> word)
            {
                if (current.empty())
                {
                    current = word;
                    continue;
                }

                if (current.size() + 1 + word.size() > max_len)
                {
                    if (current.back() != '.' && current.back() != '!' && current.back() != '?' && current.back() != ',' && current.back() != ';' && current.back() != ':')
                    {
                        current.push_back(',');
                    }
                    chunks.push_back(current);
                    current = word;
                }
                else
                {
                    current.push_back(' ');
                    current += word;
                }
            }

            if (!current.empty())
            {
                if (current.back() != '.' && current.back() != '!' && current.back() != '?' && current.back() != ',' && current.back() != ';' && current.back() != ':')
                {
                    current.push_back(',');
                }
                chunks.push_back(current);
            }
        };

        for (std::size_t i = 0; i < text.size(); ++i)
        {
            if (text[i] == '.' || text[i] == '!' || text[i] == '?')
            {
                flush_sentence(sentence);
                sentence.clear();
            }
            else
            {
                sentence.push_back(text[i]);
            }
        }

        if (!sentence.empty())
        {
            flush_sentence(sentence);
        }

        if (chunks.empty() && !text.empty())
        {
            std::string fallback = text;
            trim(fallback);
            if (!fallback.empty())
            {
                if (fallback.back() != '.' && fallback.back() != '!' && fallback.back() != '?')
                {
                    fallback.push_back('.');
                }
                chunks.push_back(std::move(fallback));
            }
        }

        return chunks;
    }
}
