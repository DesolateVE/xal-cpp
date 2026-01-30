#pragma once
#include <filesystem>
#include <string>
#include <windows.h>
#include <random>

namespace std {
// UTF-8 字符串转宽字符串
inline std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty())
        return {};
    int          size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), result.data(), size);
    return result;
}

// 宽字符串转 UTF-8 字符串
inline std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty())
        return {};
    int         size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), result.data(), size, nullptr, nullptr);
    return result;
}

inline std::filesystem::path GetRuntimePath() {
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(NULL, buffer, MAX_PATH);
    std::filesystem::path path(buffer);
    return path.parent_path();
}

inline int rand_int(int l, int r) {
    static std::mt19937                gen(std::random_device{}());
    std::uniform_int_distribution<int> dist(l, r);
    return dist(gen);
}

} // namespace std