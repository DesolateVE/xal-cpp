# MSLoginWebView 测试脚本

## 测试环境

- Windows 10/11
- WebView2 Runtime 已安装
- Visual Studio 2022

## 运行测试

### 1. 构建项目
```powershell
xmake build webview2_test
```

### 2. 运行手动模式测试
```powershell
xmake run webview2_test
```

**预期结果**：
- 窗口打开（480x640，无边框）
- 显示 Xbox Live 登录页面
- 控制台输出状态变化：
  ```
  === Testing Manual Mode ===
  [State] Initializing - Initializing WebView...
  [State] NavigatingToLogin - WebView ready, starting login flow...
  [State] WaitingUserInput - Please login manually
  ```
- 手动输入凭据完成登录
- 捕获到重定向后控制台显示授权码

### 3. 测试自动模式

**步骤**：
1. 编辑 `src/webview2_test/main.cpp`
2. 取消注释 `TestAutomaticMode` 调用
3. 填入真实的邮箱和密码：
   ```cpp
   std::string email = "your_real_email@example.com";
   std::string password = "your_real_password";
   ```
4. 重新构建并运行：
   ```powershell
   xmake build webview2_test
   xmake run webview2_test
   ```

**预期结果**：
- 窗口打开
- 自动填写邮箱、密码
- 自动点击登录按钮
- 自动处理后续步骤
- 最终捕获授权码

### 4. 测试模式切换

**创建测试文件** `test_mode_switch.cpp`：

```cpp
#include "MSLoginWebView.hpp"
#include <iostream>

using namespace MSLogin;

const std::string XBOX_LOGIN_URL = "..."; // 完整 URL

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    MSLoginWebView loginView;
    bool switchedToManual = false;
    int autoRetryCount = 0;
    
    loginView.SetStateCallback([&](LoginState state, const std::string& msg) {
        std::cout << "[State] " << (int)state << ": " << msg << std::endl;
        
        // 自动模式失败 3 次后切换手动
        if (state == LoginState::Failed && !switchedToManual) {
            autoRetryCount++;
            if (autoRetryCount >= 3) {
                std::cout << "Auto mode failed 3 times, switching to manual..." << std::endl;
                loginView.SwitchToManualMode();
                switchedToManual = true;
            }
        }
    });
    
    loginView.SetResultCallback([](const LoginResult& result) {
        if (result.success) {
            std::cout << "\n=== SUCCESS ===" << std::endl;
            std::cout << "Code: " << result.authorizationCode << std::endl;
        }
    });
    
    loginView.SetWindowParams(hInstance, nCmdShow);
    loginView.StartLogin(
        LoginMode::Automatic,
        XBOX_LOGIN_URL,
        "test@example.com",
        "wrong_password",  // 故意使用错误密码测试失败处理
        false
    );
    
    return 0;
}
```

## 测试检查清单

### 基本功能
- [ ] 窗口正常打开
- [ ] 窗口尺寸正确（480x640）
- [ ] 窗口无边框
- [ ] 窗口居中显示
- [ ] 登录页面正确加载

### 手动模式
- [ ] 进入 WaitingUserInput 状态
- [ ] 可以手动输入凭据
- [ ] 能捕获重定向
- [ ] 正确提取授权码
- [ ] ResultCallback 被调用

### 自动模式
- [ ] autologin.js 成功加载
- [ ] 脚本成功注入
- [ ] 邮箱自动填写
- [ ] 密码自动填写
- [ ] 按钮自动点击
- [ ] 状态正确转换
- [ ] 授权码正确提取

### 网络监控
- [ ] DevTools Protocol 正确启用
- [ ] Network.responseReceived 事件触发
- [ ] 重定向 URL 正确捕获
- [ ] URL 参数正确解析

### 回调机制
- [ ] StateCallback 在每次状态变化时触发
- [ ] ResultCallback 在完成时触发
- [ ] 回调参数正确

### 错误处理
- [ ] 空 URL 正确拒绝
- [ ] 自动模式缺少凭据正确拒绝
- [ ] autologin.js 缺失正确报错
- [ ] WebView2 初始化失败正确处理

### 模式切换
- [ ] SwitchToManualMode 正确工作
- [ ] 状态正确转换到 WaitingUserInput
- [ ] 网络监控继续工作

### 线程安全
- [ ] 异步模式不阻塞主线程
- [ ] WaitForCompletion 正确阻塞
- [ ] 回调在正确的线程执行

## 调试技巧

### 查看 DevTools
在 `OnWebViewReady` 中添加：
```cpp
m_webview->OpenDevToolsWindow();
```

### 输出所有网络事件
在 `HandleDevToolsEvent` 开头添加：
```cpp
std::cout << "Network: " << eventJson << std::endl;
```

### 测试脚本注入
在浏览器控制台运行：
```javascript
console.log(_UserEmail, _UserPwd);
```

### 验证状态转换
添加详细日志：
```cpp
loginView.SetStateCallback([](LoginState state, const std::string& msg) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::cout << std::ctime(&time) << " [State] " << (int)state << ": " << msg << std::endl;
});
```

## 已知问题

1. **首次运行可能较慢**
   - 原因：WebView2 需要下载组件
   - 解决：耐心等待首次初始化

2. **脚本执行时机**
   - 问题：autologin.js 可能在页面加载前执行
   - 解决：使用 AddScriptToExecuteOnDocumentCreated 确保正确时机

3. **多因素认证**
   - 问题：自动模式无法处理 MFA
   - 解决：使用手动模式或扩展脚本

## 性能指标

- **窗口启动时间**：~1-2 秒
- **脚本注入时间**：<100 毫秒
- **自动登录时间**：5-15 秒（取决于网络）
- **手动登录时间**：用户操作时间 + 捕获重定向 <500 毫秒

## 测试环境配置

### 最小配置
- Windows 10 1809+
- WebView2 Runtime
- 512MB 可用内存

### 推荐配置
- Windows 11
- WebView2 Runtime (最新版)
- 1GB 可用内存
- 稳定的网络连接

## 自动化测试

可以编写自动化测试脚本：

```powershell
# test_all.ps1
$tests = @(
    "test_manual_mode",
    "test_automatic_mode",
    "test_mode_switch",
    "test_error_handling"
)

foreach ($test in $tests) {
    Write-Host "Running $test..."
    xmake run $test
    if ($LASTEXITCODE -ne 0) {
        Write-Host "FAILED: $test" -ForegroundColor Red
        exit 1
    }
    Write-Host "PASSED: $test" -ForegroundColor Green
}

Write-Host "All tests passed!" -ForegroundColor Green
```

## 报告问题

如果遇到问题，请提供：
1. 操作系统版本
2. WebView2 Runtime 版本
3. 完整的错误消息
4. 复现步骤
5. 控制台输出日志
