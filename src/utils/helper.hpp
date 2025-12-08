#pragma once
#include <string>
#include <windows.h>

namespace std
{
    // UTF-8 字符串转宽字符串
    inline std::wstring Utf8ToWide(const std::string &str)
    {
        if (str.empty())
            return {};
        int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
        std::wstring result(size, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), result.data(), size);
        return result;
    }

    // 宽字符串转 UTF-8 字符串
    inline std::string WideToUtf8(const std::wstring &wstr)
    {
        if (wstr.empty())
            return {};
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
        std::string result(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), result.data(), size, nullptr, nullptr);
        return result;
    }
}