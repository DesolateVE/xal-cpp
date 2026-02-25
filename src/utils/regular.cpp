#include "regular.hpp"

#include <format>
#include <regex>

static int hexValue(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

std::string Regular::urlDecode(const std::string& value) {
    std::string result;
    result.reserve(value.size());

    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()) {
            int hi = hexValue(value[i + 1]);
            int lo = hexValue(value[i + 2]);

            if (hi >= 0 && lo >= 0) {
                char decoded = static_cast<char>((hi << 4) | lo);
                result.push_back(decoded);
                i += 2;
                continue;
            }
        }

        // 非法或普通字符：原样保留
        result.push_back(value[i]);
    }

    return result;
}

std::string Regular::extractHtmlBody(const std::string& raw) {
    std::regex  bodyRegex(R"xx(<body[^>]*>(.*?)</body>)xx", std::regex::icase);
    std::smatch match;
    if (std::regex_search(raw, match, bodyRegex)) {
        return match[1].str();
    }
    return "";
}

std::map<std::string, std::string> Regular::extractInputNameValue(const std::string& raw) {
    std::map<std::string, std::string> result;
    std::regex                         inputRegex(R"xx(<input\s+[^>]*name=["']([^"']+)["'][^>]*value=["']([^"']*)["'][^>]*>)xx", std::regex::icase);
    auto                               begin = std::sregex_iterator(raw.begin(), raw.end(), inputRegex);
    auto                               end   = std::sregex_iterator();

    for (auto it = begin; it != end; ++it) {
        std::smatch match = *it;
        if (match.size() == 3) {
            result[match[1].str()] = match[2].str();
        }
    }
    return result;
}

std::map<std::string, std::string> Regular::extractUrlParameters(const std::string& url) {
    std::map<std::string, std::string> result;
    std::regex                         paramRegex(R"xx([?&]([^=]+)=([^&]+))xx");
    auto                               begin = std::sregex_iterator(url.begin(), url.end(), paramRegex);
    auto                               end   = std::sregex_iterator();

    for (auto it = begin; it != end; ++it) {
        std::smatch match = *it;
        if (match.size() == 3) {
            result[match[1].str()] = match[2].str();
        }
    }
    return result;
}

std::string Regular::extractServerDataJson(const std::string& raw) {
    std::regex  serverDataRegex(R"xx(var\s+ServerData\s*=\s*(\{.*?\});)xx");
    std::smatch match;
    if (std::regex_search(raw, match, serverDataRegex)) {
        return match[1].str();
    }
    return "";
}

Regular::FormData Regular::extractFormData(const std::string& raw) {
    FormData result;

    // 提取 action
    std::regex  actionRegex(R"xx(<form\s+[^>]*action="([^"]+)"[^>]*>)xx", std::regex::icase);
    std::smatch actionMatch;
    if (std::regex_search(raw, actionMatch, actionRegex)) {
        result.action = actionMatch[1].str();
    }

    // 提取 method
    std::regex  methodRegex(R"xx(<form\s+[^>]*method="([^"]+)"[^>]*>)xx", std::regex::icase);
    std::smatch methodMatch;
    if (std::regex_search(raw, methodMatch, methodRegex)) {
        result.method = methodMatch[1].str();
    }

    // 提取所有 input 的 name/value
    result.inputs = extractInputNameValue(raw);

    return result;
}

std::string Regular::extractTagContent(const std::string& raw, const std::string& tag) {
    std::regex  tagRegex(std::format(R"xx(<{0}[^>]*>(.*?)</{0}>)xx", tag), std::regex::icase);
    std::smatch match;
    if (std::regex_search(raw, match, tagRegex)) {
        return match[1].str();
    }
    return "";
}

std::string Regular::extractTagAttribute(const std::string& raw, const std::string& tag, const std::string& attribute) {
    std::regex  tagAttrRegex(std::format(R"xx(<{0}[^>]*{1}=["']([^"']+)["'][^>]*>)xx", tag, attribute), std::regex::icase);
    std::smatch match;
    if (std::regex_search(raw, match, tagAttrRegex)) {
        return match[1].str();
    }
    return "";
}