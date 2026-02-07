#pragma once
#include <map>
#include <string>

namespace Regular {

/// @brief 对 URL 进行解码，例如将 %20 解码为一个空格
std::string urlDecode(const std::string& value);

/// @brief 从原始 HTML 中提取 <body>标签内的内容
std::string extractHtmlBody(const std::string& raw);

/// @brief 从原始 HTML 中提取 <input type="hidden" name="pprid" id="pprid" value="b7a196b9a80679ab"> 中的 name 和 value，返回一个 map
std::map<std::string, std::string> extractInputValues(const std::string& raw);

/// @brief 从原始 HTML 中提取 <form action="https://example.com/login" method="post"> 中的 action URL
std::string extractActionUrl(const std::string& raw);

/// @brief 从 URL 中提取参数，例如 https://example.com/callback?code=123&state=abc，返回一个 map 包含 code=123 和 state=abc
std::map<std::string, std::string> extractUrlParameters(const std::string& url);

} // namespace Regular