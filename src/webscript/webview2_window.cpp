#include "webview2_window.hpp"

#include <shlobj.h> // SHGetKnownFolderPath

#include "../utils/helper.hpp"
#include "../utils/logger.hpp"

// 自定义窗口消息
#define WM_WEBVIEW_NAVIGATE (WM_APP + 1)      // 导航到 URL
#define WM_WEBVIEW_EXEC_SCRIPT (WM_APP + 3)   // 执行脚本（同步）
#define WM_WEBVIEW_INJECT_SCRIPT (WM_APP + 4) // 注入文档创建时脚本

WebView2Window::WebView2Window(const WindowParams& params)
    : m_params(params) {}

WebView2Window::~WebView2Window() {
    Close();
    Join();
}

bool WebView2Window::CreateWindowInternal() {
    // 注册窗口类（显式使用 W 版本，避免受 UNICODE 宏影响）
    const wchar_t CLASS_NAME[] = L"WebView2SimpleWindow";

    WNDCLASSW wc     = {};
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = m_params.hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH); // 使用黑色背景避免白边
    wc.hCursor       = LoadCursorW(nullptr, MAKEINTRESOURCEW(IDC_ARROW));

    RegisterClassW(&wc);

    // 创建窗口（无标题栏/边框，禁止调整大小）
    DWORD style = NULL;

    // 如果有父窗口，添加 WS_CHILD 样式
    if (m_params.parent) {
        style = WS_CHILD | WS_VISIBLE;
    }

    m_hWnd = CreateWindowExW(
        m_params.parent ? 0 : WS_EX_APPWINDOW, // 子窗口不需要 WS_EX_APPWINDOW
        CLASS_NAME,
        L"",
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        m_params.width,
        m_params.height,
        m_params.parent, // 父窗口句柄
        nullptr,
        m_params.hInstance,
        this // 传递 this 指针
    );

    if (!m_hWnd) {
        return false;
    }

    ShowWindow(m_hWnd, m_params.nCmdShow);
    UpdateWindow(m_hWnd);

    // 仅当是顶层窗口且无边框时才居中显示
    if (m_params.borderless && !m_params.parent) {
        // 居中显示
        RECT rc;
        GetWindowRect(m_hWnd, &rc);
        int winW    = rc.right - rc.left;
        int winH    = rc.bottom - rc.top;
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);
        int x       = (screenW - winW) / 2;
        int y       = (screenH - winH) / 2;
        SetWindowPos(m_hWnd, nullptr, x, y, winW, winH, SWP_NOZORDER | SWP_NOSIZE);
    }

    // 初始化 WebView2
    InitializeWebView();

    return true;
}

void WebView2Window::InitializeWebView() {
    // 使用参数中的 userDataFolder，如果为空则使用默认位置
    std::wstring userDataFolder = m_params.userDataFolder;
    
    if (userDataFolder.empty()) {
        // 默认：LocalAppData\MSLoginWebView2
        wil::unique_cotaskmem_string localAppData;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData))) {
            userDataFolder = std::wstring(localAppData.get()) + L"\\MSLoginWebView2";
        }
    }
    
    // 确保目录存在
    if (!userDataFolder.empty()) {
        CreateDirectoryW(userDataFolder.c_str(), nullptr);
    }

    // 创建 WebView2 环境
    CreateCoreWebView2EnvironmentWithOptions(
        nullptr,
        userDataFolder.empty() ? nullptr : userDataFolder.c_str(),
        nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>([this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
            if (FAILED(result)) {
                LOG_ERROR("[WebView2] 创建环境失败: 0x" + std::to_string(result));
                return result;
            }

            // 创建 WebView2 控制器
            env->CreateCoreWebView2Controller(
                m_hWnd,
                Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                    [this](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT { return InitializeController(result, controller); }
                ).Get()
            );

            return S_OK;
        }).Get()
    );
}

HRESULT WebView2Window::InitializeController(HRESULT result, ICoreWebView2Controller* controller) {
    if (FAILED(result)) {
        LOG_ERROR("[WebView2] 创建控制器失败: 0x" + std::to_string(result));
        return result;
    }

    m_webviewController = controller;
    m_webviewController->get_CoreWebView2(&m_webview);

    // 设置边界
    RECT bounds;
    GetClientRect(m_hWnd, &bounds);
    m_webviewController->put_Bounds(bounds);
    m_webviewController->put_IsVisible(TRUE);

    // 配置设置
    wil::com_ptr<ICoreWebView2Settings> settings;
    m_webview->get_Settings(&settings);
    settings->put_IsScriptEnabled(TRUE);
    settings->put_AreDefaultScriptDialogsEnabled(TRUE);
    settings->put_IsWebMessageEnabled(TRUE);

    // 注册导航开始事件（可捕获自定义协议如 ms-xal-://）
    m_webview->add_NavigationStarting(
        Callback<ICoreWebView2NavigationStartingEventHandler>(
            [this](ICoreWebView2* sender, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                wil::unique_cotaskmem_string uri;
                args->get_Uri(&uri);
                std::string url = std::WideToUtf8(uri.get());

                if (m_navigationStartingCallback) {
                    bool cancel = m_navigationStartingCallback(url);
                    if (cancel) {
                        args->put_Cancel(TRUE);
                    }
                }
                return S_OK;
            }
        ).Get(),
        nullptr
    );

    // 打开 DevTools 便于调试（可注释掉）
    // if (m_webview)
    // {
    //     m_webview->OpenDevToolsWindow();
    // }

    // 标记就绪并通知等待者
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_isReady = true;
    }
    m_cv.notify_all();

    // 调用就绪回调
    if (m_readyCallback) {
        m_readyCallback();
    }

    return S_OK;
}

bool WebView2Window::WaitForReady(int timeoutMs) {
    if (m_isReady) {
        return true;
    }

    std::unique_lock<std::mutex> lock(m_mutex);

    if (timeoutMs < 0) {
        // 无限等待
        m_cv.wait(lock, [this] { return m_isReady.load(); });
        return true;
    } else {
        // 超时等待
        return m_cv.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this] { return m_isReady.load(); });
    }
}

bool WebView2Window::Navigate(const std::string& url) {
    if (!m_webview) {
        LOG_ERROR("[导航] WebView2 未初始化");
        return false;
    }

    std::wstring wUrl = std::Utf8ToWide(url);

    // 线程安全：通过 PostMessage 在 UI 线程执行
    if (m_thread && std::this_thread::get_id() != m_thread->get_id()) {
        // 从其他线程调用：投递到 UI 线程
        std::wstring* pUrl = new std::wstring(wUrl);
        PostMessage(m_hWnd, WM_WEBVIEW_NAVIGATE, 0, reinterpret_cast<LPARAM>(pUrl));
        return true;
    }

    // UI 线程直接调用
    HRESULT hr = m_webview->Navigate(wUrl.c_str());
    if (FAILED(hr)) {
        LOG_ERROR("[导航] 导航失败: 0x" + std::to_string(hr));
        return false;
    }

    return true;
}

// ==================== 线程安全的脚本执行接口 ====================

// 脚本执行 Payload（内部使用）
struct ScriptExecPayload {
    std::wstring            script;
    std::wstring            result;
    std::mutex              mtx;
    std::condition_variable cv;
    bool                    done = false;
    HRESULT                 hr   = S_OK;
};

// 脚本注入 Payload（内部使用）
struct ScriptInjectPayload {
    std::wstring            script;
    std::mutex              mtx;
    std::condition_variable cv;
    bool                    done = false;
    HRESULT                 hr   = S_OK;
};

bool WebView2Window::ExecuteScriptSync(const std::string& script, std::string& result) {
    if (!m_webview || !m_hWnd) {
        LOG_ERROR("[执行脚本] WebView2 未初始化");
        return false;
    }

    ScriptExecPayload payload;
    payload.script = std::Utf8ToWide(script);

    // 投递消息到 UI 线程
    if (!PostMessage(m_hWnd, WM_WEBVIEW_EXEC_SCRIPT, 0, reinterpret_cast<LPARAM>(&payload))) {
        LOG_ERROR("[执行脚本] PostMessage 失败");
        return false;
    }

    // 等待结果（最多 30 秒）
    {
        std::unique_lock<std::mutex> lock(payload.mtx);
        if (!payload.cv.wait_for(lock, std::chrono::seconds(30), [&payload] { return payload.done; })) {
            LOG_ERROR("[执行脚本] 超时");
            return false;
        }
    }

    if (FAILED(payload.hr)) {
        LOG_ERROR("[执行脚本] 失败: 0x" + std::to_string(payload.hr));
        return false;
    }

    result = std::WideToUtf8(payload.result);
    return true;
}

bool WebView2Window::AddScriptToExecuteOnDocumentCreated(const std::string& script) {
    if (!m_webview || !m_hWnd) {
        LOG_ERROR("[注入脚本] WebView2 未初始化");
        return false;
    }

    ScriptInjectPayload payload;
    payload.script = std::Utf8ToWide(script);

    // 投递消息到 UI 线程
    if (!PostMessage(m_hWnd, WM_WEBVIEW_INJECT_SCRIPT, 0, reinterpret_cast<LPARAM>(&payload))) {
        LOG_ERROR("[注入脚本] PostMessage 失败");
        return false;
    }

    // 等待结果（最多 10 秒）
    {
        std::unique_lock<std::mutex> lock(payload.mtx);
        if (!payload.cv.wait_for(lock, std::chrono::seconds(10), [&payload] { return payload.done; })) {
            LOG_ERROR("[注入脚本] 超时");
            return false;
        }
    }

    if (FAILED(payload.hr)) {
        LOG_ERROR("[注入脚本] 失败: 0x" + std::to_string(payload.hr));
        return false;
    }

    return true;
}

void WebView2Window::Close() {
    if (m_hWnd) {
        // 跨线程：发送消息
        m_shouldExit = true;
        PostMessage(m_hWnd, WM_CLOSE, 0, 0);
    }
}

void WebView2Window::Join() {
    if (m_thread && m_thread->joinable()) {
        m_thread->join();
    }
}

void WebView2Window::ThreadProc() {
    // 初始化 COM（单线程套间）
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        LOG_ERROR("[线程] COM 初始化失败: 0x" + std::to_string(hr));
        return;
    }

    // 创建窗口
    if (CreateWindowInternal()) {
        // 运行消息循环
        MessageLoop();
    }

    // 清理
    if (m_webviewController) {
        m_webviewController->Close();
    }
    m_webviewController = nullptr;
    m_webview           = nullptr;

    CoUninitialize();
}

int WebView2Window::MessageLoop() {
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return static_cast<int>(msg.wParam);
}

void WebView2Window::Run() {
    // 确保设置了参数
    if (!m_params.hInstance) {
        m_params.hInstance = GetModuleHandle(nullptr);
    }

    // 异步：在独立线程创建窗口
    if (m_hWnd || m_thread) {
        LOG_ERROR("[运行] 窗口已创建");
        return;
    }
    m_thread = std::make_unique<std::thread>(&WebView2Window::ThreadProc, this);
}

LRESULT CALLBACK WebView2Window::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    WebView2Window* pWindow = nullptr;

    if (uMsg == WM_NCCREATE) {
        // 从创建参数中获取 this 指针
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pWindow               = reinterpret_cast<WebView2Window*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pWindow));
    } else {
        pWindow = reinterpret_cast<WebView2Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (pWindow) {
        switch (uMsg) {
        case WM_SIZE:
            if (pWindow->m_webviewController) {
                RECT bounds;
                GetClientRect(hwnd, &bounds);
                pWindow->m_webviewController->put_Bounds(bounds);
            }
            return 0;

        case WM_DPICHANGED:
            // 当 DPI 改变时，按照系统建议的矩形调整窗口大小和位置
            if (pWindow->m_webviewController) {
                RECT* suggested = reinterpret_cast<RECT*>(lParam);
                SetWindowPos(
                    hwnd,
                    nullptr,
                    suggested->left,
                    suggested->top,
                    suggested->right - suggested->left,
                    suggested->bottom - suggested->top,
                    SWP_NOZORDER | SWP_NOACTIVATE
                );
                RECT bounds;
                GetClientRect(hwnd, &bounds);
                pWindow->m_webviewController->put_Bounds(bounds);
            }
            return 0;

        case WM_WEBVIEW_NAVIGATE: // 导航消息
        {
            std::wstring* pUrl = reinterpret_cast<std::wstring*>(lParam);
            if (pUrl && pWindow->m_webview) {
                pWindow->m_webview->Navigate(pUrl->c_str());
                LOG_INFO("[消息处理] 导航到: " + std::WideToUtf8(*pUrl));
                delete pUrl;
            }
        }
            return 0;

        case WM_WEBVIEW_EXEC_SCRIPT: // 在 UI 线程执行脚本（同步返回）
        {
            auto* payload = reinterpret_cast<ScriptExecPayload*>(lParam);
            if (payload && pWindow->m_webview) {
                pWindow->m_webview->ExecuteScript(
                    payload->script.c_str(),
                    Callback<ICoreWebView2ExecuteScriptCompletedHandler>([payload](HRESULT error, LPCWSTR resultObjectAsJson) -> HRESULT {
                        std::unique_lock<std::mutex> lock(payload->mtx);
                        payload->hr = error;
                        if (SUCCEEDED(error) && resultObjectAsJson) {
                            payload->result = resultObjectAsJson;
                        }
                        payload->done = true;
                        payload->cv.notify_one();
                        return S_OK;
                    }).Get()
                );
            }
        }
            return 0;

        case WM_WEBVIEW_INJECT_SCRIPT: // 在 UI 线程注入文档创建时脚本
        {
            auto* payload = reinterpret_cast<ScriptInjectPayload*>(lParam);
            if (payload && pWindow->m_webview) {
                HRESULT hr = pWindow->m_webview->AddScriptToExecuteOnDocumentCreated(
                    payload->script.c_str(),
                    Callback<ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler>([payload](HRESULT error, PCWSTR id) -> HRESULT {
                        std::unique_lock<std::mutex> lock(payload->mtx);
                        payload->hr   = error;
                        payload->done = true;
                        payload->cv.notify_one();
                        return S_OK;
                    }).Get()
                );
                if (FAILED(hr)) {
                    std::unique_lock<std::mutex> lock(payload->mtx);
                    payload->hr   = hr;
                    payload->done = true;
                    payload->cv.notify_one();
                }
            }
        }
            return 0;

        case WM_CLOSE:
            pWindow->m_shouldExit = true;
            if (pWindow->m_webviewController) {
                pWindow->m_webviewController->Close();
            }
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY: PostQuitMessage(0); return 0;
        }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
