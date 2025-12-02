#pragma once

#include <windows.h>
#include <wrl.h>
#include <wil/com.h>
#include <WebView2.h>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

using namespace Microsoft::WRL;

class WebView2Window
{
public:
    struct WindowParams
    {
        HINSTANCE hInstance = nullptr;
        int nCmdShow = SW_SHOW;
        int width = 1200;
        int height = 800;
        bool borderless = true;
        bool privateMode = false; // 是否使用无痕模式（InPrivate）
        HWND parent = nullptr;
    };

    WebView2Window(const WindowParams &params);
    ~WebView2Window();

    // 运行窗口（异步在独立线程运行）
    void Run();

    // 关闭窗口
    void Close();

    // 等待窗口线程结束
    void Join();
    // 获取 WebView2 接口
    wil::com_ptr<ICoreWebView2> GetWebView() const { return m_webview; }

    // 获取窗口句柄
    HWND GetHwnd() const { return m_hWnd; }

    // 设置就绪回调（在 WebView2 初始化完成时调用）
    void SetReadyCallback(std::function<void()> callback) { m_readyCallback = callback; }

    // 设置导航监听回调（可捕获所有导航，包括自定义协议如 ms-xal-://）
    // 回调返回 true 表示已处理（取消导航），返回 false 表示继续
    void SetNavigationStartingCallback(std::function<bool(const std::string &url)> callback) { m_navigationStartingCallback = callback; }

    // 阻塞等待 WebView2 就绪（返回是否成功初始化）
    bool WaitForReady(int timeoutMs = -1);

    // 导航到指定 URL（线程安全，UTF-8）
    bool Navigate(const std::string &url);

    // 执行 JS 脚本并同步获取返回值（线程安全，可从任意线程调用）
    bool ExecuteScriptSync(const std::string &script, std::string &result);

    // 注入文档创建时脚本（线程安全）
    bool AddScriptToExecuteOnDocumentCreated(const std::string &script);

private:
    // 窗口过程
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // 初始化 WebView2
    void InitializeWebView();

    // 初始化 Controller（InPrivate 和普通模式共用）
    HRESULT InitializeController(HRESULT result, ICoreWebView2Controller *controller);

    // 创建窗口（内部使用）
    bool CreateWindowInternal();

    // 线程入口函数
    void ThreadProc();

    // 消息循环
    int MessageLoop();

private:
    HWND m_hWnd = nullptr;
    wil::com_ptr<ICoreWebView2Controller> m_webviewController;
    wil::com_ptr<ICoreWebView2> m_webview;

    // 线程相关
    std::unique_ptr<std::thread> m_thread;
    std::atomic<bool> m_isReady{false};
    std::atomic<bool> m_shouldExit{false};
    std::mutex m_mutex;
    std::condition_variable m_cv;

    // 就绪回调
    std::function<void()> m_readyCallback;

    // 导航回调
    std::function<bool(const std::string &url)> m_navigationStartingCallback;

    // 窗口参数
    WindowParams m_params;
};
