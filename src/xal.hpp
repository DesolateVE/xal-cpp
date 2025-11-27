#pragma once

#include <openssl/evp.h>

#include <filesystem>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "utils/jwt_key.hpp"
#include "utils/token_store.hpp"

class XAL {
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

    // SisuToken moved to utils/token_store.hpp (top-level structs)

public:
    XAL(std::filesystem::path token_file);
    ~XAL();

    std::string getLoginUri();
    void        authenticateUser(std::string redirectUri);
    bool        isAuthenticating() const { return mIsAuthenticating; }

    UserToken getUserToken();
    SisuToken getSisuToken();
    MsalToken getMsalToken();
    XstsToken getWebToken();
    GSToken   getGSToken();

private:
    DeviceToken   getDeviceToken();
    CodeChallenge getCodeChallenge();
    std::string   genRandomState(int bytes);

    // 执行 Sisu 认证，返回包含用户令牌的响应
    SisuAuthResponse doSisuAuthentication(const DeviceToken&   device_token,
                                          const CodeChallenge& code_challenge,
                                          const std::string&   state);

    // 执行 Sisu 授权，返回包含授权令牌的响应
    SisuToken doSisuAuthorization(const UserToken&   user_token,
                                  const DeviceToken& device_token,
                                  const std::string& session_ID);

    // 交换 code 获取用户令牌
    UserToken exchangeCodeForToken(std::string code, std::string code_verifier);

    // 执行 XSTS 授权
    XstsToken doXstsAuthorization(const SisuToken& sisu_token, const std::string& relyingParty);

    MsalToken exchangeRefreshTokenForXcloudTransferToken(const UserToken& user_token);

    GSToken genStreamingToken(const XstsToken& xsts_token, std::string offering);

    // 刷新用户令牌
    UserToken refreshUserToken();

    // 使用 JwtKey 对数据进行签名
    std::vector<uint8_t> SignData(const std::string& url,
                                  const std::string& authorization_token,
                                  const std::string& payload,
                                  EVP_PKEY*          pkey);

private:
    std::unique_ptr<JwtKey>        mJwtKey;
    std::unique_ptr<UserToken>     mUserToken;
    std::unique_ptr<DeviceToken>   mDeviceToken;
    std::unique_ptr<CodeChallenge> mCodeChallenge;
    std::unique_ptr<SisuToken>     mSisuToken;
    std::unique_ptr<XstsToken>     mStreamingXsts;
    std::unique_ptr<XstsToken>     mWebToken;
    std::unique_ptr<GSToken>       mGSToken;
    std::filesystem::path          mTokenFilePath;
    bool                           mIsAuthenticating = false;
};
