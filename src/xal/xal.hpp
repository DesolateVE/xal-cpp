#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <string>

#include "mslogin_export.hpp"
#include "utils/jwt_key.hpp"
#include "utils/logger.hpp"
#include "utils/token_store.hpp"

class MSLOGIN_API XAL {
    struct CodeChallenge {
        std::string verifier;
        std::string value;
        std::string method;
    };

    struct Xdi {
        std::string did;
        std::string dcs;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Xdi, did, dcs)
    };

    struct DisplayClaims {
        Xdi xdi;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(DisplayClaims, xdi)
    };

    struct DeviceToken {
        std::string   IssueInstant;
        std::string   NotAfter;
        std::string   Token;
        DisplayClaims DisplayClaims;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(DeviceToken, IssueInstant, NotAfter, Token, DisplayClaims)
    };

    struct SisuAuthResponse {
        std::string                           MsaOauthRedirect;
        std::map<std::string, nlohmann::json> MsaRequestParameters;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(SisuAuthResponse, MsaOauthRedirect, MsaRequestParameters)
    };

    const nlohmann::json _app = {{"AppId", "000000004c20a908"}, {"TitleId", "328178078"}, {"RedirectUri", "ms-xal-000000004c20a908://auth"}};

public:
    XAL(std::filesystem::path token_file, std::filesystem::path device_file = "");
    ~XAL();

    std::string getLoginUri();
    void        authenticateUser(std::string redirectUri);
    bool        isAuthenticating() const { return mIsAuthenticating; }
    void        saveTokensToFile();

    XAL_UserToken getUserToken();
    SisuToken     getSisuToken();
    IMsalToken    getMsalToken();
    XstsToken     getWebToken();
    GSToken       getGSToken();

private:
    DeviceToken   getDeviceToken();
    CodeChallenge getCodeChallenge();
    std::string   genRandomState(int bytes);

    // 执行 Sisu 认证，返回包含用户令牌的响应
    SisuAuthResponse doSisuAuthentication(const DeviceToken& device_token, const CodeChallenge& code_challenge, const std::string& state);

    // 执行 Sisu 授权，返回包含授权令牌的响应
    SisuToken doSisuAuthorization(const XAL_UserToken& user_token, const DeviceToken& device_token, const std::string& session_ID);

    // 交换授权码获取用户令牌
    XAL_UserToken exchangeCodeForToken(std::string code, std::string code_verifier);

    // 执行 XSTS 授权
    XstsToken doXstsAuthorization(const SisuToken& sisu_token, const std::string& relyingParty);

    IMsalToken exchangeRefreshTokenForXcloudTransferToken(const XAL_UserToken& user_token);

    GSToken genStreamingToken(const XstsToken& xsts_token, std::string offering);

    // 刷新用户令牌
    XAL_UserToken refreshUserToken();

    // 使用 JWT 密钥对数据进行签名
    std::vector<uint8_t> SignData(const std::string& url, const std::string& authorization_token, const std::string& payload, EVP_PKEY* pkey);

private:
    void loadDeviceToken(); // 加载设备令牌（全局共享）
    void loadUserTokens();  // 加载用户令牌（每个账号独立）
    void saveDeviceToken(); // 保存设备令牌
    void saveUserTokens();  // 保存用户令牌

private:
    std::unique_ptr<JwtKey>        mJwtKey;
    std::unique_ptr<XAL_UserToken> mUserToken;
    std::unique_ptr<DeviceToken>   mDeviceToken;
    std::unique_ptr<CodeChallenge> mCodeChallenge;
    std::unique_ptr<SisuToken>     mSisuToken;
    std::unique_ptr<XstsToken>     mStreamingXsts;
    std::unique_ptr<XstsToken>     mWebToken;
    std::unique_ptr<GSToken>       mGSToken;
    std::filesystem::path          mTokenFilePath;  // 用户令牌文件路径
    std::filesystem::path          mDeviceFilePath; // 设备令牌文件路径（共享）
    bool                           mIsAuthenticating = false;
};
