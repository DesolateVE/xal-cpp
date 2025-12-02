#pragma once

#include <windows.h>
#include <wrl.h>
#include <wil/com.h>
#include <WebView2.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

class WebView2Window;

// WebView2 自动化操作封装类（类似 Playwright）
class WebView2Automation {
public:
    // 错误码定义（与 browser_automation.js 保持一致）
    enum ErrorCode {
        ERR_SUCCESS = 0,      // 成功
        ERR_EXCEPTION = 1,    // 处理过程中发生异常
        ERR_NO_HANDLER = 2,   // 没有对应处理函数
        ERR_NOT_FOUND = 3,    // 元素未找到
        ERR_TIMEOUT = 4       // 超时
    };

    // 接收 WebView2Window 引用
    explicit WebView2Automation(WebView2Window& window);

    // 注入辅助 JS 脚本（browser_automation.js）
    bool InjectHelperScript();

    // ===== 登录自动化 =====

    // 设置登录凭据
    bool SetCredentials(const std::string& username, const std::string& password);

    // 执行指定页面的 action
    // title: 页面标题，outMessage: 返回错误信息
    // 返回错误码: 0=成功, 1=异常, 2=无处理函数
    ErrorCode DoAction(const std::string& title, std::string* outMessage = nullptr);

    // ===== 基础操作 =====

    // 查询元素是否存在
    bool QuerySelector(const std::string& selector);

    // 点击元素
    bool Click(const std::string& selector, std::string* error = nullptr);

    // 通过文本点击按钮
    bool ClickButton(const std::string& buttonText, std::string* error = nullptr);

    // 输入文本到输入框
    bool Type(const std::string& selector, const std::string& value, std::string* error = nullptr);

    // 获取元素文本内容
    bool GetText(const std::string& selector, std::string& outText);

    // 获取输入框的值
    bool GetValue(const std::string& selector, std::string& outValue);

    // 等待元素出现（同步阻塞）
    bool WaitForSelector(const std::string& selector, int timeoutMs = 5000, std::string* error = nullptr);

    // ===== Microsoft 登录页专用 =====

    // 获取当前页面标题（Microsoft 登录页的当前步骤）
    bool GetPageTitle(std::string& outTitle);

    // 列出所有可见按钮
    bool ListButtons(std::vector<std::string>& outButtons);

    // ===== 辅助方法 =====

    // 执行 JS 脚本并同步获取返回值（阻塞等待）
    bool ExecuteScriptSync(const std::string& script, std::string& result);

private:
    WebView2Window& m_window;

    // 解析 JSON 结果
    bool ParseJsonResult(const std::string& jsonStr, json& outJson);
};
