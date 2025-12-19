#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

// ==================== GetCredentialType 请求结构体定义 ====================

struct GET_CREDENTIAL_TYPE_PAYLOAD {
    bool        checkPhones     = false;                // 是否检查电话号码
    std::string country         = "";                   // 国家/地区代码
    int         federationFlags = 11;                   // 联合身份验证配置标志
    std::string flowToken;                              // 当前身份验证流程的令牌
    bool        forceotclogin                  = false; // 是否强制一次性代码登录
    bool        isCookieBannerShown            = false; // Cookie横幅是否已显示
    bool        isExternalFederationDisallowed = false; // 是否禁止外部联合身份验证
    bool        isFederationDisabled           = false; // 是否禁用联合身份验证
    bool        isFidoSupported                = false; // 是否支持FIDO身份验证
    bool        isOtherIdpSupported            = false; // 是否支持其他身份提供程序
    bool        isReactLoginRequest            = true;  // 是否为React登录请求
    bool        isRemoteConnectSupported       = false; // 是否支持远程连接
    bool        isRemoteNGCSupported           = false; // 是否支持远程NGC
    bool        isSignup                       = false; // 是否为注册请求
    std::string originalRequest                = "";    // 原始身份验证请求数据
    bool        otclogindisallowed             = false; // 是否禁止OTC登录
    std::string uaid;                                   // 唯一账户标识符
    std::string username;                               // 用户名或电子邮件地址
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    GET_CREDENTIAL_TYPE_PAYLOAD,
    checkPhones,
    country,
    federationFlags,
    flowToken,
    forceotclogin,
    isCookieBannerShown,
    isExternalFederationDisallowed,
    isFederationDisabled,
    isFidoSupported,
    isOtherIdpSupported,
    isReactLoginRequest,
    isRemoteConnectSupported,
    isRemoteNGCSupported,
    isSignup,
    originalRequest,
    otclogindisallowed,
    uaid,
    username
)

// ==================== GetCredentialType 响应结构体定义 ====================

struct OtcLoginEligibleProof {
    std::string data;        // 加密的凭据数据
    bool        isDefault;   // 是否为默认验证方式
    bool        isNopa;      // 是否为无密码验证
    int         type;        // 类型：1=邮箱, 2=短信, 等
    std::string clearDigits; // 明文显示的部分（如 "zh"）
    std::string display;     // 用于显示的混淆值（如 "zh*****@163.com"）
    bool        otcEnabled;  // 一次性代码是否启用
    bool        otcSent;     // 一次性代码是否已发送
    bool        isLost;      // 是否标记为丢失
    bool        isSleeping;  // 是否休眠
    bool        isSADef;     // 是否为安全账户默认
    bool        pushEnabled; // 推送是否启用
};

struct Credential {
    int                                PrefCredential;         // 首选凭据类型：1=密码, 2=短信, 3=邮箱, 等
    int                                HasPassword;            // 是否有密码：1=有, 0=无
    int                                HasRemoteNGC;           // 是否有远程 Windows Hello
    int                                HasFido;                // 是否有 FIDO 硬件密钥
    int                                HasPhone;               // 是否有手机号
    int                                OTCNotAutoSent;         // 一次性代码未自动发送标志
    bool                               CobasiApp;              // 是否为 Cobasi 应用
    int                                HasGitHubFed;           // 是否有 GitHub 联合登录
    int                                HasGoogleFed;           // 是否有 Google 联合登录
    int                                HasLinkedInFed;         // 是否有 LinkedIn 联合登录
    int                                HasAppleFed;            // 是否有 Apple 联合登录
    std::string                        SendOTTHR;              // 一次性代码发送阈值
    std::vector<OtcLoginEligibleProof> OtcLoginEligibleProofs; // 可用的一次性登录验证方式
};

struct GET_CREDENTIAL_TYPE_RESPONSE {
    std::string Username;                    // 用户名
    std::string Display;                     // 显示名称
    std::string Location;                    // 位置信息
    int         IfExistsResult;              // 账户存在状态：0=存在, 1=不存在, 2=需要重定向, 等
    bool        AliasDisabledForLogin;       // 别名是否禁用登录
    Credential  Credentials;                 // 凭据信息
    bool        FullPasswordResetExperience; // 是否为完整密码重置体验
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    OtcLoginEligibleProof,
    data,
    isDefault,
    isNopa,
    type,
    clearDigits,
    display,
    otcEnabled,
    otcSent,
    isLost,
    isSleeping,
    isSADef,
    pushEnabled
)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    Credential,
    PrefCredential,
    HasPassword,
    HasRemoteNGC,
    HasFido,
    HasPhone,
    OTCNotAutoSent,
    CobasiApp,
    HasGitHubFed,
    HasGoogleFed,
    HasLinkedInFed,
    HasAppleFed,
    SendOTTHR,
    OtcLoginEligibleProofs
)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    GET_CREDENTIAL_TYPE_RESPONSE,
    Username,
    Display,
    Location,
    IfExistsResult,
    AliasDisabledForLogin,
    Credentials,
    FullPasswordResetExperience
)
