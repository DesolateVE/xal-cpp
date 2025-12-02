#include "WebView2Automation.hpp"
#include "webview2_window.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>

WebView2Automation::WebView2Automation(WebView2Window &window)
    : m_window(window)
{
}

bool WebView2Automation::InjectHelperScript()
{
    // 读取 browser_automation.js 文件
    std::ifstream file("browser_automation.js");
    if (!file.is_open())
    {
        std::cerr << "[Automation] Failed to open browser_automation.js" << std::endl;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string scriptContent = buffer.str();

    // 调用 WebView2Window 的线程安全接口
    if (!m_window.AddScriptToExecuteOnDocumentCreated(scriptContent))
    {
        std::cerr << "[Automation] Failed to inject helper script" << std::endl;
        return false;
    }

    std::cout << "[Automation] Helper script injected successfully" << std::endl;
    return true;
}

bool WebView2Automation::ExecuteScriptSync(const std::string &script, std::string &result)
{
    // 直接调用 WebView2Window 的线程安全接口
    if (!m_window.ExecuteScriptSync(script, result))
    {
        return false;
    }

    // 去除 JSON 字符串的首尾引号（ExecuteScript 返回的是 JSON 编码的字符串）
    if (result.length() >= 2 && result.front() == '"' && result.back() == '"')
    {
        result = result.substr(1, result.length() - 2);
        // 解码转义字符
        size_t pos = 0;
        while ((pos = result.find("\\\"", pos)) != std::string::npos)
        {
            result.replace(pos, 2, "\"");
            pos += 1;
        }
    }

    return true;
}

bool WebView2Automation::ParseJsonResult(const std::string &jsonStr, json &outJson)
{
    try
    {
        outJson = json::parse(jsonStr);
        return true;
    }
    catch (const json::parse_error &e)
    {
        std::cerr << "[Automation] JSON parse error: " << e.what() << std::endl;
        std::cerr << "[Automation] JSON string: " << jsonStr << std::endl;
        return false;
    }
}

bool WebView2Automation::QuerySelector(const std::string &selector)
{
    std::string script = "window.__automation.querySelector('" + selector + "')";
    std::string result;

    if (!ExecuteScriptSync(script, result))
    {
        return false;
    }

    json j;
    if (!ParseJsonResult(result, j))
    {
        return false;
    }

    return j["errcode"].get<int>() == ERR_SUCCESS;
}

bool WebView2Automation::Click(const std::string &selector, std::string *error)
{
    std::string script = "window.__automation.click('" + selector + "')";
    std::string result;

    if (!ExecuteScriptSync(script, result))
    {
        if (error)
            *error = "ExecuteScript failed";
        return false;
    }

    json j;
    if (!ParseJsonResult(result, j))
    {
        if (error)
            *error = "JSON parse failed";
        return false;
    }

    int errcode = j["errcode"].get<int>();
    if (errcode == ERR_SUCCESS)
    {
        return true;
    }
    else
    {
        if (error && j.contains("message"))
        {
            *error = j["message"].get<std::string>();
        }
        return false;
    }
}

bool WebView2Automation::ClickButton(const std::string &buttonText, std::string *error)
{
    std::string script = "window.__automation.clickButton('" + buttonText + "')";
    std::string result;

    if (!ExecuteScriptSync(script, result))
    {
        if (error)
            *error = "ExecuteScript failed";
        return false;
    }

    json j;
    if (!ParseJsonResult(result, j))
    {
        if (error)
            *error = "JSON parse failed";
        return false;
    }

    int errcode = j["errcode"].get<int>();
    if (errcode == ERR_SUCCESS)
    {
        return true;
    }
    else
    {
        if (error && j.contains("message"))
        {
            *error = j["message"].get<std::string>();
        }
        return false;
    }
}

bool WebView2Automation::Type(const std::string &selector, const std::string &value, std::string *error)
{
    // 转义单引号
    std::string escapedValue = value;
    size_t pos = 0;
    while ((pos = escapedValue.find("'", pos)) != std::string::npos)
    {
        escapedValue.replace(pos, 1, "\\'");
        pos += 2;
    }

    std::string script = "window.__automation.type('" + selector + "', '" + escapedValue + "')";
    std::string result;

    if (!ExecuteScriptSync(script, result))
    {
        if (error)
            *error = "ExecuteScript failed";
        return false;
    }

    json j;
    if (!ParseJsonResult(result, j))
    {
        if (error)
            *error = "JSON parse failed";
        return false;
    }

    int errcode = j["errcode"].get<int>();
    if (errcode == ERR_SUCCESS)
    {
        std::cout << "[Automation] Typed into " << selector << ": " << value << std::endl;
        return true;
    }
    else
    {
        if (error && j.contains("message"))
        {
            *error = j["message"].get<std::string>();
        }
        return false;
    }
}

bool WebView2Automation::GetText(const std::string &selector, std::string &outText)
{
    std::string script = "window.__automation.getText('" + selector + "')";
    std::string result;

    if (!ExecuteScriptSync(script, result))
    {
        return false;
    }

    json j;
    if (!ParseJsonResult(result, j))
    {
        return false;
    }

    if (j["errcode"].get<int>() == ERR_SUCCESS)
    {
        outText = j["text"].get<std::string>();
        return true;
    }
    return false;
}

bool WebView2Automation::GetValue(const std::string &selector, std::string &outValue)
{
    std::string script = "window.__automation.getValue('" + selector + "')";
    std::string result;

    if (!ExecuteScriptSync(script, result))
    {
        return false;
    }

    json j;
    if (!ParseJsonResult(result, j))
    {
        return false;
    }

    if (j["errcode"].get<int>() == ERR_SUCCESS)
    {
        outValue = j["value"].get<std::string>();
        return true;
    }
    return false;
}

bool WebView2Automation::WaitForSelector(const std::string &selector, int timeoutMs, std::string *error)
{
    std::string script = "await window.__automation.waitForSelector('" + selector + "', " + std::to_string(timeoutMs) + ")";
    std::string result;

    if (!ExecuteScriptSync(script, result))
    {
        if (error)
            *error = "ExecuteScript failed";
        return false;
    }

    json j;
    if (!ParseJsonResult(result, j))
    {
        if (error)
            *error = "JSON parse failed";
        return false;
    }

    int errcode = j["errcode"].get<int>();
    if (errcode == ERR_SUCCESS)
    {
        return true;
    }
    else
    {
        if (error && j.contains("message"))
        {
            *error = j["message"].get<std::string>();
        }
        return false;
    }
}

bool WebView2Automation::GetPageTitle(std::string &outTitle)
{
    std::string script = "window.__automation.getPageTitle()";
    std::string result;

    if (!ExecuteScriptSync(script, result))
    {
        return false;
    }

    json j;
    if (!ParseJsonResult(result, j))
    {
        return false;
    }

    if (j["errcode"].get<int>() == ERR_SUCCESS)
    {
        outTitle = j["title"].get<std::string>();
        return true;
    }
    return false;
}

bool WebView2Automation::ListButtons(std::vector<std::string> &outButtons)
{
    std::string script = "window.__automation.listButtons()";
    std::string result;

    if (!ExecuteScriptSync(script, result))
    {
        return false;
    }

    json j;
    if (!ParseJsonResult(result, j))
    {
        return false;
    }

    if (j["errcode"].get<int>() == ERR_SUCCESS)
    {
        outButtons.clear();
        for (const auto &btn : j["buttons"])
        {
            outButtons.push_back(btn.get<std::string>());
        }
        return true;
    }
    return false;
}

bool WebView2Automation::SetCredentials(const std::string &username, const std::string &password)
{
    // 转义单引号
    auto escapeString = [](const std::string &str)
    {
        std::string escaped = str;
        size_t pos = 0;
        while ((pos = escaped.find("'", pos)) != std::string::npos)
        {
            escaped.replace(pos, 1, "\\'");
            pos += 2;
        }
        return escaped;
    };

    std::string script = "window.__automation.setCredentials('" +
                         escapeString(username) + "', '" +
                         escapeString(password) + "')";
    std::string result;

    if (!ExecuteScriptSync(script, result))
    {
        std::cerr << "[Automation] SetCredentials: ExecuteScript failed" << std::endl;
        return false;
    }

    json j;
    if (!ParseJsonResult(result, j))
    {
        std::cerr << "[Automation] SetCredentials: JSON parse failed" << std::endl;
        return false;
    }

    if (j["errcode"].get<int>() == ERR_SUCCESS)
    {
        std::cout << "[Automation] Credentials set successfully" << std::endl;
        return true;
    }

    return false;
}

WebView2Automation::ErrorCode WebView2Automation::DoAction(const std::string &title, std::string *outMessage)
{
    // 转义单引号
    std::string escapedTitle = title;
    size_t pos = 0;
    while ((pos = escapedTitle.find("'", pos)) != std::string::npos)
    {
        escapedTitle.replace(pos, 1, "\\'");
        pos += 2;
    }

    std::string script = "window.__automation.doAction('" + escapedTitle + "')";
    std::string result;

    if (!ExecuteScriptSync(script, result))
    {
        if (outMessage)
            *outMessage = "ExecuteScript failed";
        return ERR_EXCEPTION;
    }

    json j;
    if (!ParseJsonResult(result, j))
    {
        if (outMessage)
            *outMessage = "JSON parse failed: " + result;
        return ERR_EXCEPTION;
    }

    // 提取错误信息
    if (outMessage && j.contains("message"))
    {
        *outMessage = j["message"].get<std::string>();
    }

    // 返回错误码
    if (j.contains("errcode"))
    {
        int errcode = j["errcode"].get<int>();
        return static_cast<ErrorCode>(errcode);
    }

    return ERR_EXCEPTION;
}
