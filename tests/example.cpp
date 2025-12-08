#include <chrono>
#include <iostream>
#include <thread>
#include <windows.h>

#include "xal/xal.hpp"
#include "webscript/WebView2Automation.hpp"
#include "webscript/webview2_window.hpp"
#include "utils/helper.hpp"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    XAL xal("tokens.json");
    std::string loginUrl = xal.getLoginUri();
    std::string redirect_url;

    WebView2Window::WindowParams params;
    params.hInstance = hInstance;
    params.nCmdShow = nCmdShow;
    params.width = 480;
    params.height = 640;
    params.borderless = true;
    params.privateMode = true;
    params.parent = nullptr;

    WebView2Window window(params);
    WebView2Automation automation(window);

    window.Run();
    window.WaitForReady();

    // 注入辅助脚本
    automation.InjectHelperScript();

    // 导航到登录页面
    window.Navigate(loginUrl);

    // 设置登录凭据
    automation.SetCredentials("sitasysvb@hotmail.com", "HcM04Kmi");

    // 监听导航事件，捕获 ms-xal- 自定义协议获取 code
    window.SetNavigationStartingCallback([&window, &redirect_url](const std::string &url) -> bool
                                         {
        // 检查是否是 OAuth 回调 URL
        if (url.find("ms-xal-") != std::string::npos && url.find("code=") != std::wstring::npos)
        {
            std::cout << "[OAuth Callback] " << url << std::endl;
            redirect_url = url;
            return true;
        }
        return false; });

    auto const timeout = std::chrono::seconds(120);
    auto endTimePoint = std::chrono::steady_clock::now() + timeout;

    std::string lastTitle;
    while (true)
    {
        // 检查是否已获取到 redirect URL
        if (!redirect_url.empty())
        {
            std::cout << "[Main] Got redirect URL, exiting loop..." << std::endl;
            break;
        }

        // 检查超时
        if (std::chrono::steady_clock::now() >= endTimePoint)
        {
            std::cout << "[Main] Timeout reached, exiting..." << std::endl;
            break;
        }

        // 获取当前页面标题
        std::string title;
        if (!automation.GetPageTitle(title))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        // 标题未变化，跳过
        if (title == lastTitle)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        // 执行对应页面的 action
        std::string message;
        auto result = automation.DoAction(title, &message);

        switch (result)
        {
        case WebView2Automation::ERR_SUCCESS:
            // 成功执行，重置超时
            std::cout << "[Main] Action executed for page: " << title << std::endl;
            lastTitle = title;
            endTimePoint = std::chrono::steady_clock::now() + timeout;
            break;

        case WebView2Automation::ERR_NO_HANDLER:
            // 没有对应的处理函数
            std::cout << "[Main] No handler for page: " << title << std::endl;
            lastTitle = title;
            break;

        case WebView2Automation::ERR_EXCEPTION:
            // 处理过程中发生异常
            std::cout << "[Main] Exception on page: " << title << " - " << message << std::endl;
            break;
            
        default:
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    window.Close();
    window.Join();

    if (redirect_url.empty())
    {
        std::cerr << "[Main] Failed to get redirect URL" << std::endl;
        return -1;
    }

    xal.authenticateUser(redirect_url);

    auto msal = xal.getMsalToken();

    return 0;
}
