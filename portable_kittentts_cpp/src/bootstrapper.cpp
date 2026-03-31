#include <windows.h>
#include <shellapi.h>

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
    constexpr std::array<unsigned char, 8> kFooterMagic = {'K', 'T', 'T', 'S', 'Z', 'I', 'P', '1'};
    constexpr std::size_t kFooterSize = 32;
    constexpr std::size_t kModelFieldSize = 16;

    struct PackageFooter
    {
        std::uint64_t payload_size = 0;
        std::wstring model_key;
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

    std::uint64_t read_u64_le(const unsigned char* bytes)
    {
        std::uint64_t value = 0;
        for (int i = 7; i >= 0; --i)
        {
            value = (value << 8) | bytes[i];
        }
        return value;
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

    std::filesystem::path make_workspace()
    {
        wchar_t temp_buffer[MAX_PATH];
        const DWORD temp_len = GetTempPathW(static_cast<DWORD>(std::size(temp_buffer)), temp_buffer);
        if (temp_len == 0 || temp_len >= std::size(temp_buffer))
        {
            throw std::runtime_error("Failed to resolve the temporary directory.");
        }

        const std::wstring workspace_name = L"portable_kittentts_cpp-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
        std::filesystem::path workspace = std::filesystem::path(temp_buffer) / workspace_name;
        std::filesystem::create_directories(workspace / L"app");
        return workspace;
    }

    std::filesystem::path powershell_exe()
    {
        wchar_t system_root[MAX_PATH];
        const DWORD len = GetEnvironmentVariableW(L"SystemRoot", system_root, static_cast<DWORD>(std::size(system_root)));
        if (len != 0 && len < std::size(system_root))
        {
            return std::filesystem::path(system_root) / L"System32" / L"WindowsPowerShell" / L"v1.0" / L"powershell.exe";
        }
        return L"powershell.exe";
    }

    void copy_range_to_file(const std::filesystem::path& source, std::uint64_t offset, std::uint64_t length, const std::filesystem::path& destination)
    {
        std::ifstream input(source, std::ios::binary);
        if (!input)
        {
            throw std::runtime_error("Failed to open the bootstrapper image.");
        }

        input.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        std::ofstream output(destination, std::ios::binary);
        if (!output)
        {
            throw std::runtime_error("Failed to create the temporary payload archive.");
        }

        std::array<char, 1 << 16> buffer{};
        std::uint64_t remaining = length;
        while (remaining != 0)
        {
            const std::size_t chunk = static_cast<std::size_t>(std::min<std::uint64_t>(remaining, buffer.size()));
            input.read(buffer.data(), static_cast<std::streamsize>(chunk));
            const std::streamsize read = input.gcount();
            if (read <= 0)
            {
                throw std::runtime_error("Failed to extract the embedded payload.");
            }

            output.write(buffer.data(), read);
            remaining -= static_cast<std::uint64_t>(read);
        }
    }

    void expand_archive(const std::filesystem::path& archive, const std::filesystem::path& destination)
    {
        const std::wstring command = L"Expand-Archive -LiteralPath " + quote_arg(archive.wstring()) + L" -DestinationPath " + quote_arg(destination.wstring()) + L" -Force";
        std::wstring ps_args = L"-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -WindowStyle Hidden -Command " + quote_arg(command);
        std::wstring ps_command = ps_args;

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION pi{};
        std::filesystem::path ps_path = powershell_exe();
        BOOL created = CreateProcessW(
            ps_path.c_str(),
            ps_command.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &si,
            &pi);

        if (!created)
        {
            throw std::runtime_error("Failed to launch PowerShell for payload extraction.");
        }

        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exit_code = 0;
        if (!GetExitCodeProcess(pi.hProcess, &exit_code))
        {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            throw std::runtime_error("Failed to query the PowerShell exit code.");
        }

        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);

        if (exit_code != 0)
        {
            throw std::runtime_error(std::string("Payload extraction failed with exit code ") + std::to_string(exit_code));
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
}

int main()
{
    std::filesystem::path workspace;

    try
    {
        const auto self = executable_path();
        const auto footer = read_footer(self);
        const std::wstring model_key = resolve_model_key(footer, self);
        const std::vector<std::wstring> original_args = command_line_arguments();
        const std::vector<std::wstring> child_args = build_child_args(original_args, model_key);

        const std::uintmax_t image_size = std::filesystem::file_size(self);
        if (image_size < static_cast<std::uintmax_t>(kFooterSize) + footer.payload_size)
        {
            throw std::runtime_error("The embedded payload size is invalid.");
        }

        workspace = make_workspace();
        const std::filesystem::path archive_path = workspace / L"payload.zip";
        const std::filesystem::path extracted_root = workspace / L"app";

        const std::uint64_t payload_offset = static_cast<std::uint64_t>(image_size - static_cast<std::uintmax_t>(kFooterSize) - footer.payload_size);
        copy_range_to_file(self, payload_offset, footer.payload_size, archive_path);
        expand_archive(archive_path, extracted_root);

        const std::filesystem::path child_exe = extracted_root / L"kitten_tts.exe";
        if (!std::filesystem::exists(child_exe))
        {
            throw std::runtime_error("The embedded portable app is missing kitten_tts.exe.");
        }

        if (!SetEnvironmentVariableW(L"KITTEN_TTS_ROOT", extracted_root.c_str()))
        {
            throw std::runtime_error("Failed to set KITTEN_TTS_ROOT.");
        }

        const std::wstring output_root = self.parent_path().wstring();
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
            extracted_root.c_str(),
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

        std::error_code cleanup_error;
        std::filesystem::remove_all(workspace, cleanup_error);
        return static_cast<int>(exit_code);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        if (!workspace.empty())
        {
            std::error_code cleanup_error;
            std::filesystem::remove_all(workspace, cleanup_error);
        }
        return 1;
    }
}
