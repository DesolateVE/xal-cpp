# MSLoginWebView - Microsoft/Xbox Live 登录库

## 概述

MSLoginWebView 是一个基于 WebView2 的 Microsoft/Xbox Live 登录自动化库，支持两种登录模式：

- **自动模式（Automatic）**：通过注入脚本自动完成登录流程
- **手动模式（Manual）**：用户手动输入凭据完成登录

库负责窗口管理、脚本注入、网络监控和授权码提取，调用方负责重试逻辑和模式切换决策。

## 架构设计

### 核心组件

1. **MSLoginWebView**：主类，封装登录流程
2. **WebView2Window**：底层 WebView2 窗口封装
3. **DevTools Protocol**：用于网络监控和重定向捕获
4. **autologin.js**：自动登录脚本

### 登录流程

```
Idle → Initializing → NavigatingToLogin → [Automatic/Manual] → WaitingForRedirect → Completed
                                          ↓                   ↓
                                   InjectingScript    WaitingUserInput
                                          ↓
                                   ExecutingLogin
```

### 状态机

```cpp
enum class LoginState {
    Idle,                   // 空闲状态
    Initializing,          // 初始化 WebView
    InjectingScript,       // 注入脚本中（仅自动模式）
    NavigatingToLogin,     // 导航到登录页
    ExecutingLogin,        // 执行登录中（仅自动模式）
    WaitingUserInput,      // 等待用户输入（仅手动模式）
    WaitingForRedirect,    // 等待重定向
    Completed,             // 完成
    Failed                 // 失败
};
```

## API 接口

### 基本使用

```cpp
#include "MSLoginWebView.hpp"
using namespace MSLogin;

// 1. 创建实例
MSLoginWebView loginView;

// 2. 设置回调
loginView.SetStateCallback([](LoginState state, const std::string& message) {
    // 处理状态变化
});

loginView.SetResultCallback([](const LoginResult& result) {
    if (result.success) {
        std::cout << "Authorization Code: " << result.authorizationCode << std::endl;
    }
});

// 3. 设置窗口参数
loginView.SetWindowParams(hInstance, nCmdShow, 480, 640, true, nullptr);

// 4. 启动登录
std::string loginUrl = "https://login.live.com/oauth20_authorize.srf?...";
loginView.StartLogin(LoginMode::Manual, loginUrl, "", "", false);
```

### 自动模式

```cpp
MSLoginWebView loginView;

// 设置回调...

// 启动自动登录（需要提供邮箱和密码）
std::string email = "user@example.com";
std::string password = "password123";

loginView.StartLogin(
    LoginMode::Automatic,  // 自动模式
    loginUrl,
    email,
    password,
    false  // 同步模式（阻塞）
);
```

### 手动模式

```cpp
MSLoginWebView loginView;

// 设置回调...

// 启动手动登录（用户自己操作）
loginView.StartLogin(
    LoginMode::Manual,     // 手动模式
    loginUrl,
    "",                    // 邮箱留空
    "",                    // 密码留空
    true                   // 异步模式（不阻塞）
);

// 等待完成
loginView.WaitForCompletion();
```

### 运行时切换模式

```cpp
// 启动自动模式
loginView.StartLogin(LoginMode::Automatic, loginUrl, email, password, true);

// 在自动模式失败时切换到手动模式
loginView.SwitchToManualMode();
```

## LoginResult 结构

```cpp
struct LoginResult {
    bool success;                   // 是否成功
    std::string authorizationCode;  // 授权码
    std::string state;              // state 参数
    std::string errorMessage;       // 错误信息
    LoginState finalState;          // 最终状态
};
```

## 回调机制

### StateCallback

在登录流程的每个状态变化时被调用：

```cpp
void StateCallback(LoginState state, const std::string& message);
```

**使用场景**：
- 更新 UI 进度提示
- 记录日志
- 判断是否需要切换模式

### ResultCallback

在登录完成（成功或失败）时被调用：

```cpp
void ResultCallback(const LoginResult& result);
```

**使用场景**：
- 获取授权码
- 处理错误并决定重试策略
- 更新 UI 最终状态

## 工作原理

### 自动模式工作流程

1. **脚本注入**：将 `autologin.js` 注入到页面
2. **变量设置**：注入 `_UserEmail` 和 `_UserPwd` 全局变量
3. **自动执行**：脚本根据页面标题自动点击按钮、填写表单
4. **网络监控**：通过 DevTools Protocol 监控 `Network.responseReceived` 事件
5. **捕获重定向**：检测到 `ms-xal-000000004c20a908://auth?code=...` 时提取授权码

### 手动模式工作流程

1. **跳过脚本注入**：直接进入 `WaitingUserInput` 状态
2. **用户操作**：用户在窗口中手动输入凭据
3. **网络监控**：同样监控网络响应
4. **捕获重定向**：自动提取授权码并返回

### 重定向 URL 格式

```
ms-xal-000000004c20a908://auth?code=AUTHORIZATION_CODE&state=STATE_VALUE
```

**提取逻辑**：
- 检测 URL 包含 `ms-xal-` 或 `://auth`
- 使用正则表达式解析 query 参数
- 提取 `code` 和 `state` 字段

## autologin.js 脚本

脚本基于状态机设计，根据页面标题执行相应操作：

```javascript
const actions = new Map([
    ["登录", () => {
        const btn = findButtonByText("使用 Microsoft 帐户登录");
        if (btn) btn.click();
    }],
    ["输入你的密码", () => {
        document.querySelector("input[type='password']").value = _UserPwd;
        const btn = findButtonByText("登录");
        if (btn) btn.click();
    }],
    // ... 更多步骤
]);
```

**关键特性**：
- 基于页面标题的步骤匹配
- 支持多种认证方式（密码、验证码）
- 自动处理"信任此浏览器"选项
- 10 秒超时保护

## 构建配置

### xmake.lua

```lua
target("webview2_test")
    set_kind("binary")
    set_languages("clatest", "cxxlatest")
    add_files("src/webview2_test/*.cpp")
    add_includedirs("src/webview2_test")
    add_packages("vcpkg::webview2", "nlohmann_json")
    add_defines("UNICODE", "_UNICODE", "WIN32_LEAN_AND_MEAN", "NOMINMAX")
    if is_plat("windows") then
        add_syslinks("user32", "gdi32", "shell32", "imm32", "ole32", 
                     "oleaut32", "uuid", "winmm", "version", "setupapi", "advapi32")
    end
    -- 复制 autologin.js 到构建输出目录
    after_build(function (target)
        os.cp("$(projectdir)/src/webview2_test/autologin.js", 
              path.directory(target:targetfile()))
    end)
```

### 依赖项

- **WebView2**：通过 vcpkg 安装 `webview2`
- **nlohmann/json**：用于解析 DevTools Protocol JSON 响应
- **WIL**：Windows Implementation Libraries（WebView2 SDK 自带）

## 使用示例

### 示例 1：手动模式同步登录

```cpp
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    MSLoginWebView loginView;
    
    loginView.SetStateCallback([](LoginState state, const std::string& msg) {
        std::cout << "[State] " << (int)state << ": " << msg << std::endl;
    });
    
    loginView.SetResultCallback([](const LoginResult& result) {
        if (result.success) {
            std::cout << "Success! Code: " << result.authorizationCode << std::endl;
        } else {
            std::cout << "Failed: " << result.errorMessage << std::endl;
        }
    });
    
    loginView.SetWindowParams(hInstance, nCmdShow);
    loginView.StartLogin(LoginMode::Manual, XBOX_LOGIN_URL, "", "", false);
    
    return 0;
}
```

### 示例 2：自动模式异步登录

```cpp
MSLoginWebView loginView;

// 设置回调...

loginView.SetWindowParams(hInstance, nCmdShow);
loginView.StartLogin(
    LoginMode::Automatic, 
    XBOX_LOGIN_URL, 
    "user@example.com", 
    "password123", 
    true  // 异步
);

// 继续执行其他任务...

// 需要时等待完成
loginView.WaitForCompletion();
```

### 示例 3：自动失败后切换手动

```cpp
MSLoginWebView loginView;

bool switchedToManual = false;

loginView.SetStateCallback([&](LoginState state, const std::string& msg) {
    if (state == LoginState::Failed && !switchedToManual) {
        std::cout << "Auto login failed, switching to manual..." << std::endl;
        loginView.SwitchToManualMode();
        switchedToManual = true;
    }
});

loginView.SetResultCallback([](const LoginResult& result) {
    // 处理最终结果...
});

loginView.StartLogin(LoginMode::Automatic, XBOX_LOGIN_URL, email, pwd, false);
```

## 窗口参数

```cpp
void SetWindowParams(
    HINSTANCE hInstance,    // 应用程序实例句柄
    int nCmdShow,          // 窗口显示方式
    int width = 480,       // 窗口宽度
    int height = 640,      // 窗口高度
    bool borderless = true, // 是否无边框
    HWND parent = nullptr   // 父窗口句柄（nullptr=顶层窗口）
);
```

**默认参数**：
- 尺寸：480x640（移动登录页面的最佳尺寸）
- 无边框：适合嵌入式场景
- 居中显示：顶层窗口自动居中
- 子窗口模式：提供 parent 句柄可创建子窗口

## 线程模型

### 同步模式（async=false）

- **阻塞**：`StartLogin` 会阻塞直到登录完成或失败
- **适用场景**：简单的命令行工具、单线程应用

### 异步模式（async=true）

- **非阻塞**：`StartLogin` 立即返回，窗口在独立线程运行
- **等待完成**：调用 `WaitForCompletion()` 阻塞等待结果
- **适用场景**：GUI 应用、需要并发处理的场景

## 错误处理

### 常见错误

1. **初始化失败**
   - 原因：WebView2 Runtime 未安装
   - 解决：安装 WebView2 Runtime

2. **脚本加载失败**
   - 原因：autologin.js 文件未找到
   - 解决：确保 autologin.js 在可执行文件同目录

3. **授权码提取失败**
   - 原因：重定向 URL 格式不符合预期
   - 解决：检查登录 URL 是否正确

### 错误状态处理

```cpp
loginView.SetStateCallback([](LoginState state, const std::string& msg) {
    if (state == LoginState::Failed) {
        // 记录错误
        std::cerr << "Login failed: " << msg << std::endl;
        
        // 决定重试策略
        // ...
    }
});
```

## 高级功能

### 自定义重定向 URL 模式

如果登录服务使用不同的重定向 URL 格式，可以修改 `HandleDevToolsEvent` 中的检测逻辑：

```cpp
if (url.find("your-custom-scheme://") != std::string::npos) {
    // 自定义提取逻辑
}
```

### 扩展自动登录脚本

修改 `autologin.js` 添加更多步骤：

```javascript
const actions = new Map([
    // ... 现有步骤
    ["新的页面标题", () => {
        // 执行自定义操作
        const btn = findButtonByText("继续");
        if (btn) btn.click();
    }]
]);
```

## 性能优化

- **脚本缓存**：autologin.js 只在启动时加载一次
- **事件过滤**：只监听 `Network.responseReceived`，减少开销
- **延迟初始化**：WebView2 环境按需创建

## 安全建议

1. **不要硬编码凭据**：通过参数传递或安全存储
2. **保护授权码**：获取后立即使用并清理
3. **HTTPS Only**：确保登录 URL 使用 HTTPS
4. **日志脱敏**：回调中的日志不要包含敏感信息

## 调试技巧

### 启用 DevTools

在 `InitializeWebView` 后添加：

```cpp
m_webview->OpenDevToolsWindow();
```

### 监控网络请求

修改 `HandleDevToolsEvent` 添加调试输出：

```cpp
std::cout << "Network event: " << eventJson << std::endl;
```

### 状态追踪

使用 StateCallback 记录完整状态转换：

```cpp
loginView.SetStateCallback([](LoginState state, const std::string& msg) {
    // 写入日志文件...
});
```

## 常见问题

### Q: 自动模式一直停留在 ExecutingLogin？
A: 检查 autologin.js 的页面标题匹配是否正确，可能页面已更新。

### Q: 手动模式无法捕获重定向？
A: 确保 DevTools Protocol 网络监控已正确启用。

### Q: 窗口闪退？
A: 检查是否在回调中调用了 `Stop()`，确保正确的生命周期管理。

### Q: 如何支持多因素认证（MFA）？
A: 手动模式自动支持；自动模式需扩展 autologin.js 脚本。

## 许可证

本项目为内部使用，遵循项目整体许可证。

## 贡献指南

如需扩展功能：
1. 修改 `MSLoginWebView.hpp` 添加新接口
2. 在 `MSLoginWebView.cpp` 实现功能
3. 更新 `main.cpp` 添加示例
4. 更新本文档

## 更新历史

- **v1.0** (2025-11-29)
  - 初始版本
  - 支持自动/手动双模式
  - DevTools Protocol 网络监控
  - 完整状态机和回调机制
