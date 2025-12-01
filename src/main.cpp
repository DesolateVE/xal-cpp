// #include <chrono>
// #include <iostream>
// #include <thread>
// #include <windows.h>

// #include "WebView2Automation.hpp"
// #include "webview2_window.hpp"
// #include "helper.hpp"

// int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
// {
//     std::string redirect_url;

//     WebView2Window::WindowParams params;
//     params.hInstance = hInstance;
//     params.nCmdShow = nCmdShow;
//     params.width = 480;
//     params.height = 640;
//     params.borderless = true;
//     params.privateMode = true;
//     params.parent = nullptr;

//     WebView2Window window;
//     WebView2Automation automation(window);

//     window.SetWindowParams(params);
//     window.Run();
//     window.WaitForReady();

//     // 注入辅助脚本
//     automation.InjectHelperScript();

//     // 设置登录凭据
//     automation.SetCredentials(userName, password);

//     // 导航到登录页面
//     window.Navigate("https://login.live.com/oauth20_authorize.srf?lw=1&fl=dob,easi2&xsup=1&code_challenge=S3yJc85NhKUysxFscknGYvvbQd0ru4MnIVFJhDVSn28&code_challenge_method=S256&display=android_phone&state=xTnjSmBFEOgaUDm9_2FY_VQfG6PY-FgCBz-dYQGojDW_BbcBtrvU_8Xvn9Um85Amx-3wpHFCznV1CCNJSygxag&client_id=000000004C20A908&response_type=code&scope=service%3A%3Auser.auth.xboxlive.com%3A%3AMBI_SSL&redirect_uri=ms-xal-000000004c20a908%3A%2F%2Fauth&nopa=2&uaid=20346ca373b74db2aec5c016599d6e0a");

//     // 监听导航事件，捕获 ms-xal- 自定义协议获取 code
//     window.SetNavigationStartingCallback([&window, &redirect_url](const std::wstring &url) -> bool
//                                          {
//         // 检查是否是 OAuth 回调 URL
//         if (url.find(L"ms-xal-") != std::wstring::npos && url.find(L"code=") != std::wstring::npos)
//         {
//             std::cout << "[OAuth Callback] " << WideToUtf8(url) << std::endl;
//             redirect_url = WideToUtf8(url);
//             return true;
//         }
//         return false; });

//     auto const timeout = std::chrono::seconds(10);
//     auto endTimePoint = std::chrono::steady_clock::now() + timeout;

//     std::string lastTitle;
//     while (true)
//     {
//         // 检查是否已获取到 redirect URL
//         if (!redirect_url.empty())
//         {
//             std::cout << "[Main] Got redirect URL, exiting loop..." << std::endl;
//             break;
//         }

//         // 检查超时
//         if (std::chrono::steady_clock::now() >= endTimePoint)
//         {
//             std::cout << "[Main] Timeout reached, exiting..." << std::endl;
//             break;
//         }

//         // 获取当前页面标题
//         std::string title;
//         if (!automation.GetPageTitle(title))
//         {
//             std::this_thread::sleep_for(std::chrono::milliseconds(500));
//             continue;
//         }

//         // 标题未变化，跳过
//         if (title == lastTitle)
//         {
//             std::this_thread::sleep_for(std::chrono::milliseconds(500));
//             continue;
//         }

//         // 执行对应页面的 action
//         std::string message;
//         auto result = automation.DoAction(title, &message);

//         switch (result)
//         {
//         case WebView2Automation::ACTION_SUCCESS:
//             // 成功执行，重置超时
//             std::cout << "[Main] Action executed for page: " << title << std::endl;
//             lastTitle = title;
//             endTimePoint = std::chrono::steady_clock::now() + timeout;
//             break;

//         case WebView2Automation::ACTION_NO_HANDLER:
//             // 没有对应的处理函数
//             std::cout << "[Main] No handler for page: " << title << std::endl;
//             lastTitle = title;
//             break;

//         case WebView2Automation::ACTION_EXCEPTION:
//             // 处理过程中发生异常
//             std::cout << "[Main] Exception on page: " << title << " - " << message << std::endl;
//             break;
//         }

//         std::this_thread::sleep_for(std::chrono::milliseconds(500));
//     }

//     window.Close();
//     window.Join();

//     return 0;
// }
