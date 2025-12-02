#include "xal.hpp"

#include <fstream>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include "utils/ssl_utils.hpp"
#include "utils/logger.hpp"

nlohmann::json _app = {{"AppId", "000000004c20a908"}, {"TitleId", "328178078"}, {"RedirectUri", "ms-xal-000000004c20a908://auth"}};

XAL::XAL(std::filesystem::path token_file)
    : mTokenFilePath(token_file) {
    if (!std::filesystem::exists(token_file)) {
        return;
    }

    std::ifstream file(token_file);
    if (file.is_open()) {
        nlohmann::json tokens;
        file >> tokens;
        file.close();

        // 加载各个 token
        if (tokens.contains("device_token")) {
            mDeviceToken = std::make_unique<DeviceToken>(tokens["device_token"].get<DeviceToken>());
        }
        if (tokens.contains("user_token")) {
            mUserToken        = std::make_unique<UserToken>(tokens["user_token"].get<UserToken>());
            mIsAuthenticating = true;
        }
        if (tokens.contains("sisu_token")) {
            mSisuToken = std::make_unique<SisuToken>(tokens["sisu_token"].get<SisuToken>());
        }
        if (tokens.contains("web_token")) {
            mWebToken = std::make_unique<XstsToken>(tokens["web_token"].get<XstsToken>());
        }
        if (tokens.contains("gs_token")) {
            mGSToken = std::make_unique<GSToken>(tokens["gs_token"].get<GSToken>());
        }
        if (tokens.contains("jwt_key")) {
            mJwtKey = std::make_unique<JwtKey>(tokens["jwt_key"].get<JwtKey>());
            mJwtKey->Deserialize();
        }
    }
}

XAL::~XAL() { saveTokensToFile(); }

XAL::DeviceToken XAL::getDeviceToken() {
    if (mDeviceToken) {
        return *mDeviceToken;
    }

    if (!mJwtKey) {
        mJwtKey = std::make_unique<JwtKey>();
        mJwtKey->Generate();
    }

    nlohmann::json body = {
        {"Properties",
         {{"AuthMethod", "ProofOfPossession"},
          {"Id", ssl_utils::Uuid::generate_v4()},
          {"DeviceType", "Android"},
          {"SerialNumber", ssl_utils::Uuid::generate_v4()},
          {"Version", "15.0"},
          {"ProofKey", {{"use", "sig"}, {"alg", "ES256"}, {"kty", "EC"}, {"crv", "P-256"}, {"x", mJwtKey->GetX()}, {"y", mJwtKey->GetY()}}}}},
        {"RelyingParty", "http://auth.xboxlive.com"},
        {"TokenType", "JWT"}
    };

    auto        signatureRaw = SignData("https://device.auth.xboxlive.com/device/authenticate", "", body.dump(), mJwtKey->GetEVP_PKEY());
    std::string signature    = ssl_utils::Base64::encode(signatureRaw);

    httplib::Client  cli("https://device.auth.xboxlive.com");
    httplib::Headers headers = {{"Cache-Control", "no-store, must-revalidate, no-cache"}, {"x-xbl-contract-version", "1"}, {"Signature", signature}};

    auto res = cli.Post("/device/authenticate", headers, body.dump(), "application/json");
    LOG_INFO("Device authenticate response status: " + std::string(res ? std::to_string(res->status) : "0"));
    if (res && res->status == 200) {
        LOG_DEBUG("Device token response body: " + res->body);
        DeviceToken device_token = nlohmann::json::parse(res->body).get<DeviceToken>();
        mDeviceToken             = std::make_unique<DeviceToken>(device_token);
        return *mDeviceToken;
    }

    throw std::runtime_error("Failed to get device token");
}

UserToken XAL::getUserToken() {
    if (mUserToken->isExpired()) {
        mUserToken = std::make_unique<UserToken>(refreshUserToken());
    }
    return *mUserToken;
}

SisuToken XAL::getSisuToken() {
    if (mSisuToken && !mSisuToken->isExpired()) {
        return *mSisuToken;
    }
    auto deviceToken = getDeviceToken();
    auto userToken   = getUserToken();
    mSisuToken       = std::make_unique<SisuToken>(doSisuAuthorization(userToken, deviceToken, ""));
    return *mSisuToken;
}

MsalToken XAL::getMsalToken() { return exchangeRefreshTokenForXcloudTransferToken(getUserToken()); }

XstsToken XAL::getWebToken() {
    if (mWebToken && !mWebToken->isExpired()) {
        return *mWebToken;
    }
    mWebToken = std::make_unique<XstsToken>(doXstsAuthorization(getSisuToken(), "http://xboxlive.com"));
    return *mWebToken;
}

GSToken XAL::getGSToken() {
    if (mGSToken && !mGSToken->isExpired()) {
        return *mGSToken;
    }
    mStreamingXsts = std::make_unique<XstsToken>(doXstsAuthorization(getSisuToken(), "http://gssv.xboxlive.com/"));
    mGSToken       = std::make_unique<GSToken>(genStreamingToken(*mStreamingXsts, "xhome"));
    return *mGSToken;
}

XAL::CodeChallenge XAL::getCodeChallenge() {
    if (mCodeChallenge) {
        return *mCodeChallenge;
    }

    std::string          verifier = genRandomState(32);
    std::vector<uint8_t> hash(SHA256_DIGEST_LENGTH);
    const unsigned char* d = reinterpret_cast<const unsigned char*>(verifier.data());
    size_t               n = verifier.size();
    SHA256(d, n, hash.data());

    CodeChallenge challenge;
    challenge.verifier = verifier;
    challenge.value    = ssl_utils::Base64::encode_url_safe(hash);
    challenge.method   = "S256";

    mCodeChallenge = std::make_unique<CodeChallenge>(challenge);
    return *mCodeChallenge;
}

std::string XAL::genRandomState(int bytes) {
    std::vector<uint8_t>            random_bytes(bytes);
    std::random_device              rd;
    std::mt19937                    gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    for (auto& byte : random_bytes) {
        byte = static_cast<uint8_t>(dis(gen));
    }
    return ssl_utils::Base64::encode_url_safe(random_bytes);
}

std::string XAL::getLoginUri() {
    auto deviceToken   = getDeviceToken();
    auto codeChallenge = getCodeChallenge();
    auto state         = genRandomState(64);
    auto result        = doSisuAuthentication(deviceToken, codeChallenge, state);
    return result.MsaOauthRedirect;
}

void XAL::authenticateUser(std::string redirectUri) {
    auto start        = redirectUri.find("code=") + 5;
    auto end          = redirectUri.find("&", start);
    auto code         = redirectUri.substr(start, end - start);
    mUserToken        = std::make_unique<UserToken>(exchangeCodeForToken(code, getCodeChallenge().verifier));
    mSisuToken        = std::make_unique<SisuToken>(doSisuAuthorization(getUserToken(), getDeviceToken(), ""));
    mIsAuthenticating = true;
}

void XAL::saveTokensToFile() {
    // 如果文件所在目录不存在，则创建
    std::error_code ec;
    std::filesystem::create_directories(mTokenFilePath.parent_path(), ec);
    if (ec) {
        LOG_ERROR("Failed to create token directory: " + ec.message());
        return;
    }

    // 保存所有 token 到文件
    nlohmann::json tokens;
    if (mDeviceToken) {
        tokens["device_token"] = *mDeviceToken;
    }
    if (mUserToken) {
        tokens["user_token"] = *mUserToken;
    }
    if (mSisuToken) {
        tokens["sisu_token"] = *mSisuToken;
    }
    if (mWebToken) {
        tokens["web_token"] = *mWebToken;
    }
    if (mGSToken) {
        tokens["gs_token"] = *mGSToken;
    }
    if (mJwtKey) {
        tokens["jwt_key"] = *mJwtKey;
    }

    std::ofstream file(mTokenFilePath);
    if (file.is_open()) {
        file << tokens.dump(4);
        file.close();
        LOG_INFO("Tokens saved to file: " + mTokenFilePath.string());
    } else {
        LOG_ERROR("Failed to open token file for writing: " + mTokenFilePath.string());
    }
}

XAL::SisuAuthResponse XAL::doSisuAuthentication(const DeviceToken& device_token, const CodeChallenge& code_challenge, const std::string& state) {
    nlohmann::json body = {
        {"AppId", _app["AppId"].get<std::string>()},
        {"TitleId", _app["TitleId"].get<std::string>()},
        {"RedirectUri", _app["RedirectUri"].get<std::string>()},
        {"DeviceToken", device_token.Token},
        {"Sandbox", "RETAIL"},
        {"TokenType", "code"},
        {"Offers", nlohmann::json::array({"service::user.auth.xboxlive.com::MBI_SSL"})},
        {"Query",
         {{"display", "android_phone"}, {"code_challenge", code_challenge.value}, {"code_challenge_method", code_challenge.method}, {"state", state}}}
    };

    auto        signatureRaw = SignData("https://sisu.xboxlive.com/authenticate", "", body.dump(), mJwtKey->GetEVP_PKEY());
    std::string signature    = ssl_utils::Base64::encode(signatureRaw);

    httplib::Client  cli("https://sisu.xboxlive.com");
    httplib::Headers headers = {{"Cache-Control", "no-store, must-revalidate, no-cache"}, {"x-xbl-contract-version", "1"}, {"Signature", signature}};

    auto res = cli.Post("/authenticate", headers, body.dump(), "application/json");
    LOG_INFO("Sisu authenticate response status: " + std::string(res ? std::to_string(res->status) : "0"));
    if (res && res->status == 200) {
        LOG_DEBUG("Sisu auth response body: " + res->body);
        SisuAuthResponse sisu_response = nlohmann::json::parse(res->body).get<SisuAuthResponse>();
        return sisu_response;
    }

    throw std::runtime_error("Failed to do Sisu authentication");
}

SisuToken XAL::doSisuAuthorization(const UserToken& user_token, const DeviceToken& device_token, const std::string& session_ID) {
    nlohmann::json body = {
        {"AccessToken", "t=" + user_token.access_token},
        {"AppId", _app["AppId"].get<std::string>()},
        {"DeviceToken", device_token.Token},
        {"Sandbox", "RETAIL"},
        {"SiteName", "user.auth.xboxlive.com"},
        {"UseModernGamertag", true},
        {"ProofKey", {{"use", "sig"}, {"alg", "ES256"}, {"kty", "EC"}, {"crv", "P-256"}, {"x", mJwtKey->GetX()}, {"y", mJwtKey->GetY()}}}
    };
    if (!session_ID.empty()) {
        body["SessionId"] = session_ID;
    }

    auto        signatureRaw = SignData("https://sisu.xboxlive.com/authorize", "", body.dump(), mJwtKey->GetEVP_PKEY());
    std::string signature    = ssl_utils::Base64::encode(signatureRaw);

    httplib::Client  cli("https://sisu.xboxlive.com");
    httplib::Headers headers = {{"Cache-Control", "no-store, must-revalidate, no-cache"}, {"x-xbl-contract-version", "1"}, {"Signature", signature}};

    auto res = cli.Post("/authorize", headers, body.dump(), "application/json");
    LOG_INFO("Sisu authorize response status: " + std::string(res ? std::to_string(res->status) : "0"));
    if (res && res->status == 200) {
        LOG_DEBUG("Sisu auth token response body: " + res->body);
        SisuToken sisu_token = nlohmann::json::parse(res->body).get<SisuToken>();
        return sisu_token;
    }

    throw std::runtime_error("Failed to do Sisu authorization");
}

UserToken XAL::exchangeCodeForToken(std::string code, std::string code_verifier) {
    nlohmann::json payload = {
        {"client_id", _app["AppId"].get<std::string>()},
        {"code", code},
        {"code_verifier", code_verifier},
        {"grant_type", "authorization_code"},
        {"redirect_uri", _app["RedirectUri"].get<std::string>()},
        {"scope", "service::user.auth.xboxlive.com::MBI_SSL"}
    };

    std::string      body = ssl_utils::url_encode(payload);
    httplib::Client  cli("https://login.live.com");
    httplib::Headers headers = {{"Content-Type", "application/x-www-form-urlencoded"}, {"Cache-Control", "no-store, must-revalidate, no-cache"}};

    auto res = cli.Post("/oauth20_token.srf", headers, body, "application/x-www-form-urlencoded");
    LOG_INFO("Exchange code response status: " + std::string(res ? std::to_string(res->status) : "0"));
    if (res && res->status == 200) {
        LOG_DEBUG("User token response body: " + res->body);
        UserToken user_token = nlohmann::json::parse(res->body).get<UserToken>();
        user_token.updateExpiry();
        return user_token;
    }

    LOG_ERROR("Failed to exchange code for token");
    throw std::runtime_error("Failed to exchange code for token");
}

std::vector<uint8_t> XAL::SignData(const std::string& url, const std::string& authorization_token, const std::string& payload, EVP_PKEY* pkey) {
    std::string path_and_query = url;
    auto        slash_pos      = url.find("://");
    if (slash_pos != std::string::npos) {
        auto path_start = url.find("/", slash_pos + 3);
        if (path_start != std::string::npos) {
            path_and_query = url.substr(path_start);
        }
    }

    ssl_utils::WriteBuffer singnBuffer;
    singnBuffer.write_uint32_be(1);
    singnBuffer.write_uint8(0);
    singnBuffer.write_uint64_be(ssl_utils::Time::get_windows_timestamp());
    singnBuffer.write_uint8(0);
    singnBuffer.write_string("POST");
    singnBuffer.write_uint8(0);
    singnBuffer.write_string(path_and_query);
    singnBuffer.write_uint8(0);
    singnBuffer.write_string(authorization_token);
    singnBuffer.write_uint8(0);
    singnBuffer.write_string(payload);
    singnBuffer.write_uint8(0);

    // 使用 JwtKey 对数据进行签名
    auto signature = ssl_utils::Signature::easy_sign(singnBuffer.data(), pkey);

    ssl_utils::WriteBuffer finalBuffer;
    finalBuffer.write_uint32_be(1);
    finalBuffer.write_uint64_be(ssl_utils::Time::get_windows_timestamp());
    finalBuffer.write_bytes(signature);

    return finalBuffer.data();
}

XstsToken XAL::doXstsAuthorization(const SisuToken& sisu_token, const std::string& relyingParty) {
    nlohmann::json body = {
        {"Properties",
         {{"SandboxId", "RETAIL"},
          {"DeviceToken", sisu_token.DeviceToken},
          {"TitleToken", sisu_token.TitleToken.Token},
          {"UserTokens", nlohmann::json::array({sisu_token.UserToken.Token})}}},
        {"RelyingParty", relyingParty},
        {"TokenType", "JWT"}
    };

    auto             signatureRaw = SignData("https://xsts.auth.xboxlive.com/xsts/authorize", "", body.dump(), mJwtKey->GetEVP_PKEY());
    std::string      signature    = ssl_utils::Base64::encode(signatureRaw);
    httplib::Client  cli("https://xsts.auth.xboxlive.com");
    httplib::Headers headers = {{"Cache-Control", "no-store, must-revalidate, no-cache"}, {"x-xbl-contract-version", "1"}, {"Signature", signature}};

    auto res = cli.Post("/xsts/authorize", headers, body.dump(), "application/json");
    LOG_INFO("XSTS authorize response status: " + std::string(res ? std::to_string(res->status) : "0"));
    if (res && res->status == 200) {
        LOG_DEBUG("XSTS token response body: " + res->body);
        XstsToken xsts_token = nlohmann::json::parse(res->body).get<XstsToken>();
        return xsts_token;
    }

    throw std::runtime_error("Failed to do XSTS authorization");
}

MsalToken XAL::exchangeRefreshTokenForXcloudTransferToken(const UserToken& user_token) {
    // 构造负载
    nlohmann::json payload = {
        {"client_id", _app["AppId"].get<std::string>()},
        {"grant_type", "refresh_token"},
        {"scope", "service::http://Passport.NET/purpose::PURPOSE_XBOX_CLOUD_CONSOLE_TRANSFER_TOKEN"},
        {"refresh_token", user_token.refresh_token},
        {"code", ""},
        {"code_verifier", ""},
        {"redirect_uri", ""},
    };

    // x-www-form-urlencoded 编码
    std::string body = ssl_utils::url_encode(payload);

    httplib::Client  cli("https://login.live.com");
    httplib::Headers headers = {
        {"Content-Type", "application/x-www-form-urlencoded"},
        {"Cache-Control", "no-store, must-revalidate, no-cache"},
    };

    auto res = cli.Post("/oauth20_token.srf", headers, body, "application/x-www-form-urlencoded");
    std::cout << "Response status: " << (res ? res->status : 0) << std::endl;
    if (res && res->status == 200) {
        std::cout << res->body << std::endl;
        MsalToken msal_token = nlohmann::json::parse(res->body).get<MsalToken>();
        return msal_token;
    }

    throw std::runtime_error("Failed to exchange refresh token for xcloud transfer token");
}

GSToken XAL::genStreamingToken(const XstsToken& xsts_token, std::string offering) {
    nlohmann::json body = {
        {"token", xsts_token.Token},
        {"offeringId", offering},
    };

    httplib::Client  cli("https://" + offering + ".gssv-play-prod.xboxlive.com");
    httplib::Headers headers = {
        {"Content-Type", "application/json"},
        {"Cache-Control", "no-store, must-revalidate, no-cache"},
        {"x-gssv-client", "XboxComBrowser"},
        {"Content-Length", std::to_string(body.dump().size())},
    };

    auto res = cli.Post("/v2/login/user", headers, body.dump(), "application/json");
    LOG_INFO("Streaming token response status: " + std::string(res ? std::to_string(res->status) : "0"));
    if (res && res->status == 200) {
        LOG_DEBUG("GS token response body: " + res->body);
        GSToken gs_token = nlohmann::json::parse(res->body).get<GSToken>();
        gs_token.updateExpiry();
        return gs_token;
    }

    throw std::runtime_error("Failed to generate streaming token");
}

UserToken XAL::refreshUserToken() {
    if (!mUserToken) {
        throw std::runtime_error("No user token to refresh");
    }

    nlohmann::json payload = {
        {"client_id", _app["AppId"].get<std::string>()},
        {"grant_type", "refresh_token"},
        {"refresh_token", mUserToken->refresh_token},
        {"scope", "service::user.auth.xboxlive.com::MBI_SSL"},
    };

    std::string      body = ssl_utils::url_encode(payload);
    httplib::Client  cli("https://login.live.com");
    httplib::Headers headers = {{"Content-Type", "application/x-www-form-urlencoded"}, {"Cache-Control", "no-store, must-revalidate, no-cache"}};

    auto res = cli.Post("/oauth20_token.srf", headers, body, "application/x-www-form-urlencoded");
    LOG_INFO("Refresh token response status: " + std::string(res ? std::to_string(res->status) : "0"));
    if (res && res->status == 200) {
        LOG_DEBUG("Refreshed user token response body: " + res->body);
        UserToken user_token = nlohmann::json::parse(res->body).get<UserToken>();
        user_token.updateExpiry();
        return user_token;
    }

    LOG_ERROR("Failed to refresh user token");
    throw std::runtime_error("Failed to refresh user token");
}
