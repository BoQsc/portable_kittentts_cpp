#include <windows.h>
#include <shellapi.h>

#include "cli_help.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include <cwctype>

namespace
{
    constexpr std::array<unsigned char, 8> kFooterMagic = {'K', 'T', 'T', 'S', 'R', 'A', 'W', '1'};
    constexpr std::size_t kFooterSize = 32;
    constexpr std::size_t kModelFieldSize = 16;

    struct PackageFooter
    {
        std::uint64_t payload_size = 0;
        std::wstring model_key;
    };

    struct CacheContext
    {
        std::filesystem::path root;
        std::filesystem::path app_root;
        std::filesystem::path marker;
        bool ready = false;
    };

    std::filesystem::path executable_path()
    {
        wchar_t buffer[32768];
        const DWORD len = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
        if (len == 0 || len >= std::size(buffer))
        {
            throw std::runtime_error("Failed to resolve the bootstrapper path.");
        }
        return std::filesystem::path(buffer);
    }

    std::uint32_t read_u32_le(const unsigned char* bytes)
    {
        std::uint32_t value = 0;
        for (int i = 3; i >= 0; --i)
        {
            value = (value << 8) | bytes[i];
        }
        return value;
    }

    std::uint64_t read_u64_le(const unsigned char* bytes)
    {
        std::uint64_t value = 0;
        for (int i = 7; i >= 0; --i)
        {
            value = (value << 8) | bytes[i];
        }
        return value;
    }

    std::wstring to_lower(std::wstring value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
            return static_cast<wchar_t>(std::towlower(ch));
        });
        return value;
    }

    std::wstring fallback_model_from_name(const std::filesystem::path& self_path)
    {
        const std::wstring stem = to_lower(self_path.stem().wstring());
        if (stem.find(L"nano-int8") != std::wstring::npos)
        {
            return L"nano-int8";
        }
        if (stem.find(L"mini") != std::wstring::npos)
        {
            return L"mini";
        }
        if (stem.find(L"micro") != std::wstring::npos)
        {
            return L"micro";
        }
        return L"nano";
    }

    PackageFooter read_footer(const std::filesystem::path& self_path)
    {
        std::ifstream file(self_path, std::ios::binary);
        if (!file)
        {
            throw std::runtime_error("Failed to open the bootstrapper image.");
        }

        file.seekg(0, std::ios::end);
        const std::streamoff file_size = file.tellg();
        if (file_size < static_cast<std::streamoff>(kFooterSize))
        {
            throw std::runtime_error("The bootstrapper payload footer is missing.");
        }

        file.seekg(file_size - static_cast<std::streamoff>(kFooterSize), std::ios::beg);
        std::array<unsigned char, kFooterSize> footer{};
        file.read(reinterpret_cast<char*>(footer.data()), static_cast<std::streamsize>(footer.size()));
        if (!file)
        {
            throw std::runtime_error("Failed to read the bootstrapper footer.");
        }

        if (!std::equal(kFooterMagic.begin(), kFooterMagic.end(), footer.begin()))
        {
            throw std::runtime_error("Invalid bootstrapper footer magic.");
        }

        PackageFooter parsed;
        parsed.payload_size = read_u64_le(footer.data() + 8);

        std::string model_ascii(reinterpret_cast<const char*>(footer.data() + 16), kModelFieldSize);
        const std::size_t nul_pos = model_ascii.find('\0');
        if (nul_pos != std::string::npos)
        {
            model_ascii.resize(nul_pos);
        }

        parsed.model_key.reserve(model_ascii.size());
        for (unsigned char ch : model_ascii)
        {
            parsed.model_key.push_back(static_cast<wchar_t>(ch));
        }

        return parsed;
    }

    std::vector<std::wstring> command_line_arguments()
    {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argv == nullptr)
        {
            throw std::runtime_error("Failed to parse the command line.");
        }

        std::vector<std::wstring> args;
        for (int i = 1; i < argc; ++i)
        {
            args.emplace_back(argv[i]);
        }

        LocalFree(argv);
        return args;
    }

    bool help_requested(const std::vector<std::wstring>& args)
    {
        for (const auto& arg : args)
        {
            if (arg == L"--help" || arg == L"-h" || arg == L"/?")
            {
                return true;
            }
        }
        return false;
    }

    bool coldstart_requested(const std::vector<std::wstring>& args)
    {
        for (const auto& arg : args)
        {
            if (arg == L"--coldstart" || arg == L"--cold-start")
            {
                return true;
            }
        }
        return false;
    }

    std::wstring quote_arg(const std::wstring& arg)
    {
        if (arg.empty())
        {
            return L"\"\"";
        }

        const bool needs_quotes = arg.find_first_of(L" \t\n\v\"") != std::wstring::npos;
        if (!needs_quotes)
        {
            return arg;
        }

        std::wstring quoted;
        quoted.push_back(L'"');

        std::size_t backslashes = 0;
        for (wchar_t ch : arg)
        {
            if (ch == L'\\')
            {
                ++backslashes;
                continue;
            }

            if (ch == L'"')
            {
                quoted.append(backslashes * 2 + 1, L'\\');
                quoted.push_back(L'"');
                backslashes = 0;
                continue;
            }

            if (backslashes != 0)
            {
                quoted.append(backslashes, L'\\');
                backslashes = 0;
            }

            quoted.push_back(ch);
        }

        if (backslashes != 0)
        {
            quoted.append(backslashes * 2, L'\\');
        }

        quoted.push_back(L'"');
        return quoted;
    }

    std::vector<std::wstring> build_child_args(const std::vector<std::wstring>& args, const std::wstring& model_key)
    {
        std::vector<std::wstring> filtered;
        filtered.reserve(args.size() + 2);
        filtered.emplace_back(L"--model");
        filtered.emplace_back(model_key);

        for (std::size_t i = 0; i < args.size(); ++i)
        {
            const std::wstring& arg = args[i];
            if (arg == L"--model")
            {
                if (i + 1 < args.size())
                {
                    ++i;
                }
                continue;
            }

            if (arg.rfind(L"--model=", 0) == 0)
            {
                continue;
            }

            if (arg == L"--coldstart" || arg == L"--cold-start")
            {
                continue;
            }

            filtered.push_back(arg);
        }

        return filtered;
    }

    std::wstring build_command_line(const std::filesystem::path& executable, const std::vector<std::wstring>& args)
    {
        std::wstring command = quote_arg(executable.wstring());
        for (const auto& arg : args)
        {
            command.push_back(L' ');
            command.append(quote_arg(arg));
        }
        return command;
    }

    std::filesystem::path local_appdata_root()
    {
        wchar_t buffer[32768];
        const DWORD len = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, static_cast<DWORD>(std::size(buffer)));
        if (len != 0 && len < std::size(buffer))
        {
            return std::filesystem::path(buffer);
        }
        return std::filesystem::temp_directory_path();
    }

    std::wstring sanitize_component(std::wstring value)
    {
        for (wchar_t& ch : value)
        {
            switch (ch)
            {
            case L'<':
            case L'>':
            case L':':
            case L'"':
            case L'/':
            case L'\\':
            case L'|':
            case L'?':
            case L'*':
                ch = L'_';
                break;
            default:
                break;
            }
        }
        return value;
    }

    CacheContext cache_context_for(const std::filesystem::path& self, const PackageFooter& footer)
    {
        const std::uintmax_t image_size = std::filesystem::file_size(self);
        const std::wstring stem = sanitize_component(self.stem().wstring());
        const std::wstring model = sanitize_component(footer.model_key.empty() ? L"nano" : footer.model_key);
        const std::filesystem::path cache_root = local_appdata_root() / L"portable_kittentts_cpp" / L"single-exe" /
            (stem + L"-" + model + L"-" + std::to_wstring(image_size) + L"-" + std::to_wstring(footer.payload_size));

        CacheContext ctx;
        ctx.root = cache_root;
        ctx.app_root = cache_root / L"app";
        ctx.marker = cache_root / L".ready";
        ctx.ready = std::filesystem::exists(ctx.marker) && std::filesystem::exists(ctx.app_root / L"kitten_tts.exe");
        return ctx;
    }

    std::filesystem::path coldstart_workspace_root(const std::filesystem::path& self, const PackageFooter& footer)
    {
        const std::wstring stem = sanitize_component(self.stem().wstring());
        const std::wstring model = sanitize_component(footer.model_key.empty() ? L"nano" : footer.model_key);
        const std::filesystem::path base = local_appdata_root() / L"portable_kittentts_cpp" / L"single-exe" / L"coldstart";

        std::error_code base_error;
        std::filesystem::create_directories(base, base_error);

        const unsigned long long pid = static_cast<unsigned long long>(GetCurrentProcessId());
        const unsigned long long tick = static_cast<unsigned long long>(GetTickCount64());
        for (unsigned int attempt = 0; attempt < 64; ++attempt)
        {
            const std::filesystem::path candidate = base / (stem + L"-" + model + L"-" + std::to_wstring(pid) + L"-" +
                std::to_wstring(tick) + L"-" + std::to_wstring(attempt));
            std::error_code candidate_error;
            if (std::filesystem::create_directories(candidate, candidate_error) || std::filesystem::exists(candidate))
            {
                return candidate;
            }
        }

        throw std::runtime_error("Failed to create a coldstart extraction workspace.");
    }

    void mark_cache_ready(const CacheContext& cache)
    {
        std::ofstream marker(cache.marker, std::ios::binary);
        if (!marker)
        {
            throw std::runtime_error("Failed to write the cache marker.");
        }
        marker << "ready";
    }

    std::wstring utf8_to_wide(const std::string& value)
    {
        const int wide_length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
        if (wide_length <= 0)
        {
            throw std::runtime_error("Invalid UTF-8 path in embedded payload.");
        }

        std::wstring wide(static_cast<std::size_t>(wide_length), L'\0');
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), wide.data(), wide_length) <= 0)
        {
            throw std::runtime_error("Failed to convert embedded path from UTF-8.");
        }
        return wide;
    }

    bool contains_parent_segment(const std::filesystem::path& path)
    {
        for (const auto& part : path)
        {
            if (part == L"..")
            {
                return true;
            }
        }
        return false;
    }

    std::filesystem::path safe_relative_path(const std::string& utf8_path)
    {
        const std::filesystem::path relative = std::filesystem::path(utf8_to_wide(utf8_path));
        if (relative.empty() || relative.is_absolute() || relative.has_root_name() || relative.has_root_directory() || contains_parent_segment(relative))
        {
            throw std::runtime_error("Embedded payload contained an invalid path.");
        }
        return relative;
    }

    void copy_stream(std::ifstream& input, std::ofstream& output, std::uint64_t bytes_to_copy)
    {
        std::array<char, 1 << 16> buffer{};
        std::uint64_t remaining = bytes_to_copy;
        while (remaining != 0)
        {
            const std::size_t chunk = static_cast<std::size_t>(std::min<std::uint64_t>(remaining, buffer.size()));
            input.read(buffer.data(), static_cast<std::streamsize>(chunk));
            const std::streamsize read = input.gcount();
            if (read <= 0)
            {
                throw std::runtime_error("Unexpected end of embedded payload.");
            }

            output.write(buffer.data(), read);
            remaining -= static_cast<std::uint64_t>(read);
        }
    }

    void extract_raw_bundle(const std::filesystem::path& source, std::uint64_t payload_offset, std::uint64_t payload_size, const std::filesystem::path& destination)
    {
        std::ifstream input(source, std::ios::binary);
        if (!input)
        {
            throw std::runtime_error("Failed to open the bootstrapper image.");
        }

        input.seekg(static_cast<std::streamoff>(payload_offset), std::ios::beg);
        if (!input)
        {
            throw std::runtime_error("Failed to seek to the embedded payload.");
        }

        std::uint64_t remaining = payload_size;
        while (remaining != 0)
        {
            if (remaining < 12)
            {
                throw std::runtime_error("Embedded payload is truncated.");
            }

            std::array<unsigned char, 12> header{};
            input.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
            if (!input)
            {
                throw std::runtime_error("Failed to read the embedded payload header.");
            }
            remaining -= static_cast<std::uint64_t>(header.size());

            const std::uint32_t path_length = read_u32_le(header.data());
            const std::uint64_t file_size = read_u64_le(header.data() + 4);
            if (remaining < static_cast<std::uint64_t>(path_length) + file_size)
            {
                throw std::runtime_error("Embedded payload is truncated.");
            }

            std::string path_bytes(static_cast<std::size_t>(path_length), '\0');
            if (path_length != 0)
            {
                input.read(path_bytes.data(), static_cast<std::streamsize>(path_bytes.size()));
                if (!input)
                {
                    throw std::runtime_error("Failed to read the embedded payload path.");
                }
            }
            remaining -= static_cast<std::uint64_t>(path_length);

            const std::filesystem::path relative_path = safe_relative_path(path_bytes);
            const std::filesystem::path out_path = destination / relative_path;
            if (!out_path.parent_path().empty())
            {
                std::filesystem::create_directories(out_path.parent_path());
            }

            std::ofstream output(out_path, std::ios::binary);
            if (!output)
            {
                throw std::runtime_error("Failed to create an extracted payload file.");
            }

            copy_stream(input, output, file_size);
            remaining -= file_size;
        }
    }

    std::wstring resolve_model_key(const PackageFooter& footer, const std::filesystem::path& self_path)
    {
        if (!footer.model_key.empty())
        {
            return footer.model_key;
        }
        return fallback_model_from_name(self_path);
    }

    struct TempWorkspaceCleanup
    {
        std::filesystem::path root;
        bool active = false;

        ~TempWorkspaceCleanup()
        {
            if (!active || root.empty())
            {
                return;
            }

            std::error_code cleanup_error;
            std::filesystem::remove_all(root, cleanup_error);
        }
    };
}

int main()
{
    try
    {
        const auto self = executable_path();
        const std::vector<std::wstring> original_args = command_line_arguments();
        const bool coldstart = coldstart_requested(original_args);

        if (help_requested(original_args))
        {
            print_cli_help(std::cout, true);
            return 0;
        }

        const auto footer = read_footer(self);
        const std::wstring model_key = resolve_model_key(footer, self);
        const std::vector<std::wstring> child_args = build_child_args(original_args, model_key);

        const std::uintmax_t image_size = std::filesystem::file_size(self);
        if (image_size < static_cast<std::uintmax_t>(kFooterSize) + footer.payload_size)
        {
            throw std::runtime_error("The embedded payload size is invalid.");
        }

        const std::uint64_t payload_offset = static_cast<std::uint64_t>(image_size - static_cast<std::uintmax_t>(kFooterSize) - footer.payload_size);
        const CacheContext cache = cache_context_for(self, footer);
        const bool use_cache = !coldstart;
        std::filesystem::path extraction_root;
        std::filesystem::path app_root;
        TempWorkspaceCleanup temp_workspace;

        if (use_cache)
        {
            app_root = cache.app_root;
            if (!cache.ready)
            {
                std::error_code cleanup_error;
                std::filesystem::remove_all(cache.root, cleanup_error);
                std::filesystem::create_directories(cache.app_root);
                extract_raw_bundle(self, payload_offset, footer.payload_size, cache.app_root);
                mark_cache_ready(cache);
            }
        }
        else
        {
            extraction_root = coldstart_workspace_root(self, footer);
            app_root = extraction_root / L"app";
            std::filesystem::create_directories(app_root);
            temp_workspace.root = extraction_root;
            temp_workspace.active = true;
            extract_raw_bundle(self, payload_offset, footer.payload_size, app_root);
        }

        const std::filesystem::path child_exe = app_root / L"kitten_tts.exe";
        if (!std::filesystem::exists(child_exe))
        {
            throw std::runtime_error("The embedded portable app is missing kitten_tts.exe.");
        }

        const std::wstring output_root = self.parent_path().wstring();
        if (!SetEnvironmentVariableW(L"KITTEN_TTS_ROOT", app_root.c_str()))
        {
            throw std::runtime_error("Failed to set KITTEN_TTS_ROOT.");
        }

        if (!SetEnvironmentVariableW(L"KITTEN_TTS_OUTPUT_ROOT", output_root.c_str()))
        {
            throw std::runtime_error("Failed to set KITTEN_TTS_OUTPUT_ROOT.");
        }

        std::wstring child_command_line = build_command_line(child_exe, child_args);
        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        std::wstring mutable_command_line = child_command_line;

        BOOL created = CreateProcessW(
            child_exe.c_str(),
            mutable_command_line.data(),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            app_root.c_str(),
            &si,
            &pi);

        if (!created)
        {
            throw std::runtime_error("Failed to launch the embedded Kitten TTS executable.");
        }

        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exit_code = 0;
        if (!GetExitCodeProcess(pi.hProcess, &exit_code))
        {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            throw std::runtime_error("Failed to query the embedded app exit code.");
        }

        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return static_cast<int>(exit_code);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
