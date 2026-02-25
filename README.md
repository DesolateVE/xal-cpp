# MSLogin — Xbox 认证库

Xbox Live 完整认证链的 C++ 实现，提供 OAuth2 → XAL → XSTS → GSSV → GSToken 全流程。

参考实现：[xal-node](https://github.com/unknownskl/xal-node)

## 构建

```powershell
xmake build MSLogin      # 构建 DLL
xmake build MSLoginTest   # 构建测试程序（非默认目标）
```

依赖：OpenSSL 3、nlohmann_json、cpp-httplib (SSL)、cpr (SSL)、Boost

## 核心类

### MSAL — 高层认证入口

```cpp
class MSAL {
public:
    MSAL(std::string email, std::string password, std::filesystem::path tokenDir);

    void fullLogin();          // 完整登录（设备码 + 账密自动化）
    void tokenLogin();         // 仅令牌登录（已有持久化令牌时）

    GSToken   getGSToken();    // 游戏流令牌（串流认证用）
    IMsalToken getMsalToken(); // Xcloud 传输令牌
};
```

### XAL — Xbox 认证库

```cpp
class XAL {
public:
    XAL(std::filesystem::path tokenFile);

    std::string getLoginUri();                    // 获取登录 URL
    void authenticateUser(std::string redirectUri); // 用回调码认证

    UserToken getUserToken();
    SisuToken getSisuToken();
    XstsToken getWebToken();
    GSToken   getGSToken();
};
```

### MSALLogin — 浏览器自动化登录

模拟浏览器完成 Microsoft 账号登录流程：
1. 获取登录页面 → 提取 sFT/urlPost
2. 提交设备验证码
3. 提交账号密码
4. 自动处理中间页面（安全提醒、通行密钥申请等）

### 令牌体系

| 令牌 | 用途 |
|------|------|
| `MSAL_OAuth2Token` | OAuth2 标准令牌 |
| `XAL_UserToken` | XAL 用户令牌 |
| `SisuToken` | Sisu 认证令牌（含 Title/User/Authorization Token）|
| `XstsToken` | XSTS 授权令牌 |
| `GSToken` | 游戏流令牌（包含 regions、gsToken、offeringSettings）|
| `IMsalToken` | MSAL 传输令牌 |

所有令牌支持 JSON 序列化、过期检查、文件持久化。

## 认证流程

```
1. 设备码认证 → MSAL_OAuth2Token (access_token, refresh_token)
2. MSALLogin 自动化浏览器登录（提交验证码 + 账密）
3. 轮询设备码授权完成
4. OAuth2 token → XSTS Authentication
5. XSTS → XBL Token
6. XBL → GSSV Token  
7. GSSV → GSToken (xhome/auth/authenticate)
```

## 日志系统

```cpp
// 设置日志回调（桥接到主程序的 spdlog）
Logger::getInstance().setLogCallback([](LogLevel level, const std::string& message) {
    spdlog::info("[MSLogin] {}", message);
});

// 宏支持 std::format 格式化
LOG_INFO("登录成功，账号: {}", email);
LOG_ERROR("认证失败: {}", error_msg);
```

## 导出

通过 `MSLOGIN_API` (`__declspec(dllexport)`) 导出 `MSAL`、`XAL`、`Logger` 及所有令牌结构体。
