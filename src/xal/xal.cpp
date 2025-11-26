#include "xal.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>

nlohmann::json _app = {
    {"AppId", "000000004c20a908"}, {"TitleId", "328178078"}, {"RedirectUri", "ms-xal-000000004c20a908://auth"}};

XAL::XAL(std::filesystem::path token_file) : mTokenFile(std::move(token_file)) {}

XAL::~XAL() {}

XAL::DeviceToken XAL::getDeviceToken() {
    if (mDeviceToken) { return *mDeviceToken; }

    if (!mJwtKey) {
        mJwtKey = std::make_unique<JwtKey>();
        mJwtKey->Generate();
    }

    nlohmann::json body = {{"Properties",
                            {{"AuthMethod", "ProofOfPossession"},
                             {"Id", ssl_utils::Uuid::generate_v3()},
                             {"DeviceType", "Android"},
                             {"SerialNumber", ssl_utils::Uuid::generate_v3()},
                             {"Version", "15.0"},
                             {"ProofKey",
                              {{"use", "sig"},
                               {"alg", "ES256"},
                               {"kty", "EC"},
                               {"crv", "P-256"},
                               {"x", mJwtKey->GetX()},
                               {"y", mJwtKey->GetY()}}}}},
                           {"RelyingParty", "http://auth.xboxlive.com"},
                           {"TokenType", "JWT"}};

    auto signatureRaw =
        SignData("https://device.auth.xboxlive.com/device/authenticate", "", body.dump(), mJwtKey->GetEVP_PKEY());
    std::string signature = ssl_utils::Base64::encode(signatureRaw);

    httplib::Client  cli("https://device.auth.xboxlive.com");
    httplib::Headers headers = {{"Cache-Control", "no-store, must-revalidate, no-cache"},
                                {"x-xbl-contract-version", "1"},
                                {"Signature", signature}};

    auto res = cli.Post("/device/authenticate", headers, body.dump(), "application/json");
    std::cout << "Response status: " << (res ? res->status : 0) << std::endl;
    if (res && res->status == 200) {
        std::cout << res->body << std::endl;
        DeviceToken device_token = nlohmann::json::parse(res->body).get<DeviceToken>();
        mDeviceToken             = std::make_unique<DeviceToken>(device_token);
        return *mDeviceToken;
    }

    throw std::runtime_error("Failed to get device token");
}

UserToken XAL::getUserToken() {
    if (mUserToken->isExpired()) { refreshUserToken(); }
    return *mUserToken;
}

SisuToken XAL::getSisuToken() {
    if (mSisuToken->isExpired()) {
        auto deviceToken = getDeviceToken();
        auto userToken   = getUserToken();
        doSisuAuthorization(userToken, deviceToken, "");
    }
    return *mSisuToken;
}

void XAL::getMsalToken() {
    exchangeRefreshTokenForXcloudTransferToken(getUserToken());
}

void XAL::getWebToken() {
    doXstsAuthorization(getSisuToken(), "http://xboxlive.com");
}

void XAL::getHomeStreamingToken() {
    doXstsAuthorization(getSisuToken(), "http://gssv.xboxlive.com/");
    // genStreamingToken(getXstsToken(), "xhome");
}

// XstsToken XAL::getXstsToken() {
//     return XstsToken();
// }

XAL::CodeChallenge XAL::getCodeChallenge() {
    if (mCodeChallenge) { return *mCodeChallenge; }

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

    for (auto& byte: random_bytes) { byte = static_cast<uint8_t>(dis(gen)); }
    return ssl_utils::Base64::encode_url_safe(random_bytes);
}

std::string XAL::getRedirectUri() {
    auto deviceToken   = getDeviceToken();
    auto codeChallenge = getCodeChallenge();
    auto state         = genRandomState(64);
    auto result        = doSisuAuthentication(deviceToken, codeChallenge, state);
    return result.MsaOauthRedirect;
}

void XAL::authenticateUser(std::string redirectUri) {
    auto start = redirectUri.find("code=") + 5;
    auto end   = redirectUri.find("&", start);
    auto code  = redirectUri.substr(start, end - start);
    exchangeCodeForToken(code, getCodeChallenge().verifier);
    doSisuAuthorization(getUserToken(), getDeviceToken(), "");
}

XAL::SisuAuthResponse XAL::doSisuAuthentication(const DeviceToken&   device_token,
                                                const CodeChallenge& code_challenge,
                                                const std::string&   state) {
    nlohmann::json body = {{"AppId", _app["AppId"].get<std::string>()},
                           {"TitleId", _app["TitleId"].get<std::string>()},
                           {"RedirectUri", _app["RedirectUri"].get<std::string>()},
                           {"DeviceToken", device_token.Token},
                           {"Sandbox", "RETAIL"},
                           {"TokenType", "code"},
                           {"Offers", nlohmann::json::array({"service::user.auth.xboxlive.com::MBI_SSL"})},
                           {"Query",
                            {{"display", "android_phone"},
                             {"code_challenge", code_challenge.value},
                             {"code_challenge_method", code_challenge.method},
                             {"state", state}}}};

    auto signatureRaw     = SignData("https://sisu.xboxlive.com/authenticate", "", body.dump(), mJwtKey->GetEVP_PKEY());
    std::string signature = ssl_utils::Base64::encode(signatureRaw);

    httplib::Client  cli("https://sisu.xboxlive.com");
    httplib::Headers headers = {{"Cache-Control", "no-store, must-revalidate, no-cache"},
                                {"x-xbl-contract-version", "1"},
                                {"Signature", signature}};

    auto res = cli.Post("/authenticate", headers, body.dump(), "application/json");
    std::cout << "Response status: " << (res ? res->status : 0) << std::endl;
    if (res && res->status == 200) {
        std::cout << res->body << std::endl;
        SisuAuthResponse sisu_response = nlohmann::json::parse(res->body).get<SisuAuthResponse>();
        return sisu_response;
    }

    throw std::runtime_error("Failed to do Sisu authentication");
}

void XAL::doSisuAuthorization(const UserToken&   user_token,
                              const DeviceToken& device_token,
                              const std::string& session_ID) {
    nlohmann::json body = {{"AccessToken", "t=" + user_token.access_token},
                           {"AppId", _app["AppId"].get<std::string>()},
                           {"DeviceToken", device_token.Token},
                           {"Sandbox", "RETAIL"},
                           {"SiteName", "user.auth.xboxlive.com"},
                           {"UseModernGamertag", true},
                           {"ProofKey",
                            {{"use", "sig"},
                             {"alg", "ES256"},
                             {"kty", "EC"},
                             {"crv", "P-256"},
                             {"x", mJwtKey->GetX()},
                             {"y", mJwtKey->GetY()}}}};
    if (!session_ID.empty()) { body["SessionId"] = session_ID; }

    auto        signatureRaw = SignData("https://sisu.xboxlive.com/authorize", "", body.dump(), mJwtKey->GetEVP_PKEY());
    std::string signature    = ssl_utils::Base64::encode(signatureRaw);

    httplib::Client  cli("https://sisu.xboxlive.com");
    httplib::Headers headers = {{"Cache-Control", "no-store, must-revalidate, no-cache"},
                                {"x-xbl-contract-version", "1"},
                                {"Signature", signature}};

    auto res = cli.Post("/authorize", headers, body.dump(), "application/json");
    std::cout << "Response status: " << (res ? res->status : 0) << std::endl;
    if (res && res->status == 200) {
        std::cout << res->body << std::endl;
        SisuToken sisu_token = nlohmann::json::parse(res->body).get<SisuToken>();
        mSisuToken           = std::make_unique<SisuToken>(sisu_token);
        return;
    }

    throw std::runtime_error("Failed to do Sisu authorization");
}

void XAL::exchangeCodeForToken(std::string code, std::string code_verifier) {
    nlohmann::json payload = {{"client_id", _app["AppId"].get<std::string>()},
                              {"code", code},
                              {"code_verifier", code_verifier},
                              {"grant_type", "authorization_code"},
                              {"redirect_uri", _app["RedirectUri"].get<std::string>()},
                              {"scope", "service::user.auth.xboxlive.com::MBI_SSL"}};

    std::string      body = ssl_utils::url_encode(payload);
    httplib::Client  cli("https://login.live.com");
    httplib::Headers headers = {{"Content-Type", "application/x-www-form-urlencoded"},
                                {"Cache-Control", "no-store, must-revalidate, no-cache"}};

    auto res = cli.Post("/oauth20_token.srf", headers, body, "application/x-www-form-urlencoded");
    std::cout << "Response status: " << (res ? res->status : 0) << std::endl;
    if (res && res->status == 200) {
        std::cout << res->body << std::endl;
        UserToken user_token = nlohmann::json::parse(res->body).get<UserToken>();
        mUserToken           = std::make_unique<UserToken>(user_token);
        mUserToken->updateExpiry();
        return;
    }

    throw std::runtime_error("Failed to exchange code for token");
}

std::vector<uint8_t> XAL::SignData(const std::string& url,
                                   const std::string& authorization_token,
                                   const std::string& payload,
                                   EVP_PKEY*          pkey) {
    std::string path_and_query = url;
    auto        slash_pos      = url.find("://");
    if (slash_pos != std::string::npos) {
        auto path_start = url.find("/", slash_pos + 3);
        if (path_start != std::string::npos) { path_and_query = url.substr(path_start); }
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

void XAL::doXstsAuthorization(const SisuToken& sisu_token, const std::string& relyingParty) {
    nlohmann::json body = {{"Properties",
                            {{"SandboxId", "RETAIL"},
                             {"DeviceToken", sisu_token.DeviceToken},
                             {"TitleToken", sisu_token.TitleToken.Token},
                             {"UserTokens", nlohmann::json::array({sisu_token.UserToken.Token})}}},
                           {"RelyingParty", relyingParty},
                           {"TokenType", "JWT"}};

    auto signatureRaw =
        SignData("https://xsts.auth.xboxlive.com/xsts/authorize", "", body.dump(), mJwtKey->GetEVP_PKEY());
    std::string      signature = ssl_utils::Base64::encode(signatureRaw);
    httplib::Client  cli("https://xsts.auth.xboxlive.com");
    httplib::Headers headers = {{"Cache-Control", "no-store, must-revalidate, no-cache"},
                                {"x-xbl-contract-version", "1"},
                                {"Signature", signature}};

    auto res = cli.Post("/xsts/authorize", headers, body.dump(), "application/json");
    std::cout << "Response status: " << (res ? res->status : 0) << std::endl;
    if (res && res->status == 200) {
        std::cout << res->body << std::endl;
        // XSTS Token response can be parsed here if needed
        return;
    }

    throw std::runtime_error("Failed to do XSTS authorization");
}

void XAL::exchangeRefreshTokenForXcloudTransferToken(const UserToken& user_token) {
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
        return;
    }

    throw std::runtime_error("Failed to exchange refresh token for xcloud transfer token");
}

void XAL::genStreamingToken(const XstsToken& xsts_token, std::string offering) {
    nlohmann::json body = {
        {"token", xsts_token.Token},
        {"offering", offering},
    };

    httplib::Client  cli("https://" + offering + ".gssv-play-prod.xboxlive.com");
    httplib::Headers headers = {
        {"Content-Type", "application/json"},
        {"Cache-Control", "no-store, must-revalidate, no-cache"},
        {"x-gssv-client", "XboxComBrowser"},
        {"Content-Length", std::to_string(body.dump().size())},
    };

    auto res = cli.Post("/v2/login/user", headers, body.dump(), "application/json");
    std::cout << "Response status: " << (res ? res->status : 0) << std::endl;
    if (res && res->status == 200) {
        std::cout << res->body << std::endl;
        return;
    }
}

void XAL::refreshUserToken() {
    if (!mUserToken) { throw std::runtime_error("No user token to refresh"); }

    nlohmann::json payload = {
        {"client_id", _app["AppId"].get<std::string>()},
        {"grant_type", "refresh_token"},
        {"refresh_token", mUserToken->refresh_token},
        {"scope", "service::user.auth.xboxlive.com::MBI_SSL"},
    };

    std::string      body = ssl_utils::url_encode(payload);
    httplib::Client  cli("https://login.live.com");
    httplib::Headers headers = {{"Content-Type", "application/x-www-form-urlencoded"},
                                {"Cache-Control", "no-store, must-revalidate, no-cache"}};

    auto res = cli.Post("/oauth20_token.srf", headers, body, "application/x-www-form-urlencoded");
    std::cout << "Response status: " << (res ? res->status : 0) << std::endl;
    if (res && res->status == 200) {
        std::cout << res->body << std::endl;
        UserToken user_token = nlohmann::json::parse(res->body).get<UserToken>();
        mUserToken           = std::make_unique<UserToken>(user_token);
        mUserToken->updateExpiry();
        return;
    }

    throw std::runtime_error("Failed to refresh user token");
}
