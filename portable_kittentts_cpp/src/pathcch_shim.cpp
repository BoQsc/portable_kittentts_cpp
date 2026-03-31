#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <cstddef>

using PathCchRemoveFileSpecFn = HRESULT (WINAPI *)(PWSTR, std::size_t);
using PathCchRemoveBackslashFn = HRESULT (WINAPI *)(PWSTR, std::size_t);

namespace
{
    template <typename Fn>
    Fn resolve_kernelbase_export(const char* name)
    {
        HMODULE module = GetModuleHandleW(L"kernelbase.dll");
        if (!module)
        {
            return nullptr;
        }

        return reinterpret_cast<Fn>(GetProcAddress(module, name));
    }
}

extern "C" __declspec(dllexport) HRESULT WINAPI PathCchRemoveFileSpec(PWSTR pszPath, std::size_t cchPath)
{
    const auto fn = resolve_kernelbase_export<PathCchRemoveFileSpecFn>("PathCchRemoveFileSpec");
    if (!fn)
    {
        return E_FAIL;
    }
    return fn(pszPath, cchPath);
}

extern "C" __declspec(dllexport) HRESULT WINAPI PathCchRemoveBackslash(PWSTR pszPath, std::size_t cchPath)
{
    const auto fn = resolve_kernelbase_export<PathCchRemoveBackslashFn>("PathCchRemoveBackslash");
    if (!fn)
    {
        return E_FAIL;
    }
    return fn(pszPath, cchPath);
}
