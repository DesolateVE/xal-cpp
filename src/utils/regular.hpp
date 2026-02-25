#pragma once
#include <map>
#include <string>

namespace Regular {

struct FormData {
    std::string                        action;
    std::string                        method;
    std::map<std::string, std::string> inputs; ///< input 元素的 name -> value 映射
};

/// @brief 对 URL 进行解码，例如将 %20 解码为一个空格
std::string urlDecode(const std::string& value);

/// @brief 从原始 HTML 中提取 <body> 标签内的内容
std::string extractHtmlBody(const std::string& raw);

/// @brief 从原始 HTML 中提取 <input> 标签的 name 和 value
std::map<std::string, std::string> extractInputNameValue(const std::string& raw);

/// @brief 从原始 HTML 中提取 <form> 的 action、method 以及所有 <input> 的 name/value
FormData extractFormData(const std::string& raw);

/// @brief 从 URL 中提取参数，例如 https://example.com/callback?code=123&state=abc，返回一个 map 包含 code=123 和 state=abc
std::map<std::string, std::string> extractUrlParameters(const std::string& url);

/// @brief 从原始 HTML 中提取 var ServerData = {...}; 中的 JSON 内容
std::string extractServerDataJson(const std::string& raw);

/// @brief 从原始 HTML 中提取指定标签中的内容
std::string extractTagContent(const std::string& raw, const std::string& tag);

/// @brief 从原始 HTML 中提取指定标签的属性值，例如提取 <a href="..."> 中的 href
std::string extractTagAttribute(const std::string& raw, const std::string& tag, const std::string& attribute);

} // namespace Regular