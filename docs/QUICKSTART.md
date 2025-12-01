# MSLoginWebView 快速入门

## 5 分钟快速开始

### 1. 手动模式（最简单）

```cpp
#include "MSLoginWebView.hpp"
using namespace MSLogin;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    MSLoginWebView loginView;
    
    // 设置回调
    loginView.SetResultCallback([](const LoginResult& result) {
        if (result.success) {
            // 获取到授权码！
            std::cout << "Code: " << result.authorizationCode << std::endl;
        }
    });
    
    // 设置窗口
    loginView.SetWindowParams(hInstance, nCmdShow);
    
    // 启动登录（用户手动输入）
    std::string loginUrl = "https://login.live.com/oauth20_authorize.srf?...";
    loginView.StartLogin(LoginMode::Manual, loginUrl, "", "", false);
    
    return 0;
}
```

**就这么简单！** 窗口会打开，用户手动登录，自动捕获授权码。

### 2. 自动模式（无需用户交互）

```cpp
MSLoginWebView loginView;

loginView.SetResultCallback([](const LoginResult& result) {
    if (result.success) {
        std::cout << "Code: " << result.authorizationCode << std::endl;
    }
});

loginView.SetWindowParams(hInstance, nCmdShow);

// 提供邮箱和密码，完全自动化
loginView.StartLogin(
    LoginMode::Automatic,
    loginUrl,
    "user@example.com",
    "password123",
    false
);
```

**完全自动！** 脚本会自动填写表单并完成登录。

## 核心概念

### 两种模式

| 模式 | 特点 | 使用场景 |
|------|------|----------|
| **Manual（手动）** | 用户操作 | 首次登录、MFA、不想存储密码 |
| **Automatic（自动）** | 脚本驱动 | 批量操作、自动化测试、免交互 |

### 两种运行方式

```cpp
// 同步（阻塞直到完成）
loginView.StartLogin(mode, url, email, pwd, false);

// 异步（立即返回）
loginView.StartLogin(mode, url, email, pwd, true);
loginView.WaitForCompletion();  // 需要时等待
```

### 回调函数

**状态回调**（可选）：
```cpp
loginView.SetStateCallback([](LoginState state, const std::string& msg) {
    std::cout << "State changed: " << msg << std::endl;
});
```

**结果回调**（必需）：
```cpp
loginView.SetResultCallback([](const LoginResult& result) {
    if (result.success) {
        // 使用 result.authorizationCode
    } else {
        // 处理错误 result.errorMessage
    }
});
```

## Xbox Live 登录 URL

```cpp
const std::string XBOX_LOGIN_URL = 
    "https://login.live.com/"
    "oauth20_authorize.srf?"
    "lw=1&fl=dob,easi2&xsup=1"
    "&code_challenge=8s51r5Y0ZajjHhBmWYGjt8E3eNqu5yX0Jd6XTEN3JpI"
    "&code_challenge_method=S256"
    "&display=android_phone"
    "&state=YOUR_STATE_VALUE"
    "&client_id=000000004C20A908"
    "&response_type=code"
    "&scope=service%3A%3Auser.auth.xboxlive.com%3A%3AMBI_SSL"
    "&redirect_uri=ms-xal-000000004c20a908%3A%2F%2Fauth"
    "&nopa=2"
    "&uaid=bf5c558a6d7a4263ab68bc19de67e663";
```

## 构建和运行

```powershell
# 构建
xmake build webview2_test

# 运行
xmake run webview2_test
```

## 文件结构

```
src/webview2_test/
├── MSLoginWebView.hpp       # 主类接口
├── MSLoginWebView.cpp       # 主类实现
├── webview2_window.hpp      # 窗口封装
├── webview2_window.cpp      # 窗口实现
├── autologin.js            # 自动登录脚本
└── main.cpp                # 示例程序
```

## 常见模式

### 模式 1：简单手动登录
```cpp
MSLoginWebView loginView;
loginView.SetResultCallback(onResult);
loginView.SetWindowParams(hInstance, nCmdShow);
loginView.StartLogin(LoginMode::Manual, url, "", "", false);
```

### 模式 2：带状态监控的自动登录
```cpp
MSLoginWebView loginView;

loginView.SetStateCallback([](LoginState state, const string& msg) {
    // 更新 UI 进度
});

loginView.SetResultCallback([](const LoginResult& result) {
    // 处理结果
});

loginView.SetWindowParams(hInstance, nCmdShow);
loginView.StartLogin(LoginMode::Automatic, url, email, pwd, true);
loginView.WaitForCompletion();
```

### 模式 3：自动失败后切换手动
```cpp
MSLoginWebView loginView;

loginView.SetStateCallback([&](LoginState state, const string& msg) {
    if (state == LoginState::Failed) {
        loginView.SwitchToManualMode();
    }
});

loginView.StartLogin(LoginMode::Automatic, url, email, pwd, false);
```

## 获取授权码后

```cpp
loginView.SetResultCallback([](const LoginResult& result) {
    if (result.success) {
        std::string code = result.authorizationCode;
        
        // 使用授权码获取访问令牌
        // POST https://login.live.com/oauth20_token.srf
        // {
        //   grant_type: "authorization_code",
        //   code: code,
        //   client_id: "000000004C20A908",
        //   ...
        // }
    }
});
```

## 调试技巧

### 查看浏览器控制台
```cpp
void OnWebViewReady() {
    // ...
    m_webview->OpenDevToolsWindow();  // 添加这行
    // ...
}
```

### 输出详细日志
```cpp
loginView.SetStateCallback([](LoginState state, const std::string& msg) {
    std::cout << "[" << std::chrono::system_clock::now() << "] "
              << "State: " << (int)state << " - " << msg << std::endl;
});
```

## 错误处理

```cpp
loginView.SetResultCallback([](const LoginResult& result) {
    if (!result.success) {
        std::cerr << "Login failed: " << result.errorMessage << std::endl;
        
        // 根据 finalState 决定重试策略
        switch (result.finalState) {
            case LoginState::Failed:
                // 重试或切换模式
                break;
        }
    }
});
```

## 窗口自定义

```cpp
// 默认：480x640，无边框，居中
loginView.SetWindowParams(hInstance, nCmdShow);

// 自定义尺寸
loginView.SetWindowParams(hInstance, nCmdShow, 600, 800);

// 有边框
loginView.SetWindowParams(hInstance, nCmdShow, 480, 640, false);

// 子窗口
loginView.SetWindowParams(hInstance, nCmdShow, 480, 640, true, parentHWND);
```

## 下一步

- 📖 阅读 [完整文档](MSLoginWebView_README.md)
- 🧪 查看 [测试指南](TESTING_GUIDE.md)
- 📝 查看 [实现总结](IMPLEMENTATION_SUMMARY.md)

## 最小示例

```cpp
#include "MSLoginWebView.hpp"
using namespace MSLogin;

int WINAPI WinMain(HINSTANCE h, HINSTANCE, LPSTR, int show) {
    MSLoginWebView login;
    login.SetResultCallback([](auto& r) { 
        if (r.success) MessageBoxA(0, r.authorizationCode.c_str(), "Code", 0); 
    });
    login.SetWindowParams(h, show);
    login.StartLogin(LoginMode::Manual, "YOUR_URL", "", "", false);
    return 0;
}
```

**仅 10 行代码！**

## FAQ

**Q: 需要安装什么？**  
A: 只需 WebView2 Runtime（Windows 11 自带）

**Q: 支持哪些浏览器？**  
A: 基于 Edge WebView2（Chromium）

**Q: 可以保存登录状态吗？**  
A: 可以，WebView2 会保存 Cookies（可配置）

**Q: 如何处理验证码？**  
A: 使用手动模式让用户输入

**Q: 性能如何？**  
A: 启动 1-2 秒，自动登录 5-15 秒

---

🎉 **开始使用吧！** 任何问题欢迎查阅详细文档。
