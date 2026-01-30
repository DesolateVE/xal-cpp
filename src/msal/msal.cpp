#include "msal.hpp"

#include <fstream>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include "msal_login.hpp"
#include "utils/logger.hpp"

static const std::string _clientId = "1f907974-e22b-4810-a9de-d9647380c97e";
static const std::string _userAgent =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36 Edg/144.0.0.0";

MSAL::MSAL(const std::filesystem::path& path, const std::string& email, const std::string& password)
    : _tokenFilePath(path),
      _email(email),
      _password(password) {

    if (std::filesystem::exists(_tokenFilePath)) {
        loadTokensFromFile();
    } else {
        fullLogin();
    }
}

MSAL::~MSAL() { saveTokensToFile(); }

MSAL_OAuth2Token MSAL::getOAuth2Token() {
    if (_oauth2Token.isExpired()) {
        try {
            refreshOAtuh2Token(_oauth2Token.refresh_token, _oauth2Token);
        } catch (...) {
            MSAL_IDeviceCodeAuth device_code_auth;
            MSALLogin            login_helper;
            doDeviceCodeAuth(device_code_auth);
            login_helper.MSALEasyLogin(device_code_auth.verification_uri, device_code_auth.user_code, _email, _password);
            doPollForDeviceCodeAuth(device_code_auth.device_code, _oauth2Token);
        }
    }

    return _oauth2Token;
}

XstsToken MSAL::getXstsToken() {
    if (_xstsToken.isExpired()) {
        doXstsAuthentication(getOAuth2Token().access_token, _xstsToken);
    }
    return _xstsToken;
}

XstsToken MSAL::getXBLToken() {
    if (_xblToken.isExpired()) {
        getXBLToken(getXstsToken().Token, _xblToken);
    }
    return _xblToken;
}

XstsToken MSAL::getGssvToken() {
    if (_gssvToken.isExpired()) {
        getGssvToken(getXstsToken().Token, _gssvToken);
    }
    return _gssvToken;
}

GSToken MSAL::getStreamToken() {
    if (_streamToken.isExpired()) {
        getStreamHomeToken(getGssvToken().Token, _streamToken);
    }
    return _streamToken;
}

void MSAL::fullLogin() {

    LOG_INFO("开始完整登录流程...");

    MSAL_IDeviceCodeAuth device_code_auth;
    doDeviceCodeAuth(device_code_auth);

    LOG_INFO(std::format("获取到登录提示：{}", device_code_auth.message));

    MSALLogin login_helper;
    login_helper.MSALEasyLogin(device_code_auth.verification_uri, device_code_auth.user_code, _email, _password);

    MSAL_OAuth2Token oauth2_token;
    for (size_t i = 0; i < 5; i++) {
        try {
            doPollForDeviceCodeAuth(device_code_auth.device_code, oauth2_token);
            oauth2_token.updateExpiry();
            _oauth2Token = oauth2_token;
            LOG_INFO("OAuth2 Token 获取成功.");
            break;
        } catch (...) { LOG_WARNING("等待用户完成设备登录授权..."); }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (oauth2_token.access_token.empty()) {
        throw std::runtime_error("设备登录授权超时，未能获取到 OAuth2 Token。");
    }

    XstsToken xsts_token;
    doXstsAuthentication(oauth2_token.access_token, xsts_token);
    _xstsToken = xsts_token;

    XstsToken xbl_token;
    getXBLToken(xsts_token.Token, xbl_token);
    _xblToken = xbl_token;

    XstsToken gssv_token;
    getGssvToken(xsts_token.Token, gssv_token);
    _gssvToken = gssv_token;

    GSToken stream_token;
    getStreamHomeToken(gssv_token.Token, stream_token);
    _streamToken.updateExpiry();
    _streamToken = stream_token;
}

void MSAL::loadTokensFromFile() {
    std::ifstream ifs(_tokenFilePath);
    if (ifs.is_open()) {
        nlohmann::json j;
        ifs >> j;
        ifs.close();

        if (j.contains("oauth2_token")) {
            _oauth2Token = j["oauth2_token"].get<MSAL_OAuth2Token>();
        }
        if (j.contains("xsts_token")) {
            _xstsToken = j["xsts_token"].get<XstsToken>();
        }
        if (j.contains("xbl_token")) {
            _xblToken = j["xbl_token"].get<XstsToken>();
        }
        if (j.contains("stream_token")) {
            _streamToken = j["stream_token"].get<GSToken>();
        }

    } else {
        throw std::runtime_error("Failed to open token file for reading: " + _tokenFilePath.string());
    }
}

void MSAL::saveTokensToFile() {
    // 检查目录和文件是否存在，不存在则创建
    std::filesystem::create_directories(_tokenFilePath.parent_path());

    nlohmann::json j;
    j["oauth2_token"] = _oauth2Token;
    j["xsts_token"]   = _xstsToken;
    j["xbl_token"]    = _xblToken;
    j["stream_token"] = _streamToken;
    std::ofstream ofs(_tokenFilePath);
    if (ofs.is_open()) {
        ofs << j.dump(4);
        ofs.close();
    } else {
        throw std::runtime_error("Failed to open token file for writing: " + _tokenFilePath.string());
    }
}

void MSAL::loginWithCredentials() {

    MSAL_IDeviceCodeAuth device_code_auth;
    doDeviceCodeAuth(device_code_auth);
}

void MSAL::doDeviceCodeAuth(MSAL_IDeviceCodeAuth& out_device_code) {

    httplib::Params payload = {
        {"client_id", _clientId},
        {"scope", "xboxlive.signin openid profile offline_access"},
    };

    httplib::Headers headers = {{"User-Agent", _userAgent}};

    httplib::Client cli("https://login.microsoftonline.com");
    httplib::Result res =
        cli.Post("/consumers/oauth2/v2.0/devicecode", headers, httplib::detail::params_to_query_str(payload), "application/x-www-form-urlencoded");
    if (res && res->status == 200) {
#ifdef _DEBUG
        std::cout << "Device Code Auth Response: " << res->body << "\n\n";
#endif
        out_device_code = nlohmann::json::parse(res->body).get<MSAL_IDeviceCodeAuth>();
    } else {
        throw std::runtime_error("Failed to get device code from MSAL");
    }
}

void MSAL::doPollForDeviceCodeAuth(const std::string& device_code, MSAL_OAuth2Token& out_token) {

    httplib::Params payload = {
        {"grant_type", "urn:ietf:params:oauth:grant-type:device_code"},
        {"client_id", _clientId},
        {"device_code", device_code},
    };

    httplib::Headers headers = {{"User-Agent", _userAgent}};

    httplib::Client cli("https://login.microsoftonline.com");
    auto res = cli.Post("/consumers/oauth2/v2.0/token", headers, httplib::detail::params_to_query_str(payload), "application/x-www-form-urlencoded");
    if (res && res->status == 200) {
#ifdef _DEBUG
        std::cout << "Device Code Verify Response: " << res->body << "\n\n";
#endif
        out_token = nlohmann::json::parse(res->body).get<MSAL_OAuth2Token>();
        out_token.updateExpiry();
    } else {
        throw std::runtime_error("Failed to poll for device code auth from MSAL");
    }
}

void MSAL::doXstsAuthentication(const std::string& oauth2_token, XstsToken& out_xsts_token) {
    nlohmann::json body = {
        {"RelyingParty", "http://auth.xboxlive.com"},
        {"TokenType", "JWT"},
        {"Properties", {{"AuthMethod", "RPS"}, {"SiteName", "user.auth.xboxlive.com"}, {"RpsTicket", "d=" + oauth2_token}}},
    };
    httplib::Headers headers =
        {{"User-Agent", _userAgent}, {"x-xbl-contract-version", "1"}, {"Origin", "https://www.xbox.com"}, {"Referer", "https://www.xbox.com"}};

    httplib::Client cli("https://user.auth.xboxlive.com");
    auto            res = cli.Post("/user/authenticate", headers, body.dump(), "application/json");
    if (res && res->status == 200) {
#ifdef _DEBUG
        std::cout << "XSTS Authentication Response: " << res->body << "\n\n";
#endif
        out_xsts_token = nlohmann::json::parse(res->body).get<XstsToken>();
    } else {
        throw std::runtime_error("Failed to do XSTS authentication from MSAL");
    }

    LOG_INFO("XSTS Token 获取成功.");
}

void MSAL::getGssvToken(const std::string& xsts_token, XstsToken& out_gssv_token) {
    doXstsAuthorization(xsts_token, "http://gssv.xboxlive.com/", out_gssv_token);
    LOG_INFO("GSSV Token 获取成功.");
}

void MSAL::getXBLToken(const std::string& xsts_token, XstsToken& out_xbl_token) {
    doXstsAuthorization(xsts_token, "http://xboxlive.com", out_xbl_token);
    LOG_INFO("XBL Token 获取成功.");
}

void MSAL::getStreamHomeToken(const std::string& gssv_token, GSToken& out_stream_token) { getStreamToken(gssv_token, "xhome", out_stream_token); }

void MSAL::doXstsAuthorization(const std::string& xsts_token, const std::string& relying_party, XstsToken& out_token) {
    nlohmann::json body = {
        {"RelyingParty", relying_party},
        {"TokenType", "JWT"},
        {"Properties", {{"SandboxId", "RETAIL"}, {"UserTokens", nlohmann::json::array({xsts_token})}}},
    };
    httplib::Headers headers =
        {{"User-Agent", _userAgent}, {"x-xbl-contract-version", "1"}, {"Origin", "https://www.xbox.com"}, {"Referer", "https://www.xbox.com"}};

    httplib::Client cli("https://xsts.auth.xboxlive.com");
    auto            res = cli.Post("/xsts/authorize", headers, body.dump(), "application/json");
    if (res && res->status == 200) {
#ifdef _DEBUG
        std::cout << "XSTS Authorization Response: " << res->body << "\n\n";
#endif
        out_token = nlohmann::json::parse(res->body).get<XstsToken>();
    } else {
        throw std::runtime_error("Failed to do XSTS authorization from MSAL");
    }
}

void MSAL::getStreamToken(const std::string& gssv_token, const std::string& offering, GSToken& out_stream_token) {
    nlohmann::json body = {
        {"token", gssv_token},
        {"offeringId", offering},
    };
    httplib::Headers headers = {{"User-Agent", _userAgent}, {"x-gssv-client", "XboxComBrowser"}};

    httplib::Client cli("https://" + offering + ".gssv-play-prod.xboxlive.com");
    auto            res = cli.Post("/v2/login/user", headers, body.dump(), "application/json");
    if (res && res->status == 200) {
#ifdef _DEBUG
        std::cout << "Stream Token Response: " << res->body << "\n\n";
#endif
        out_stream_token = nlohmann::json::parse(res->body).get<GSToken>();
        out_stream_token.updateExpiry();
    } else {
        throw std::runtime_error("Failed to get stream token from GSSV token");
    }
}

void MSAL::getXALToken(const std::string& refresh_token, XAL_UserToken& out_token) {

    httplib::Params payload = {
        {"client_id", _clientId},
        {"scope", "service::http://Passport.NET/purpose::PURPOSE_XBOX_CLOUD_CONSOLE_TRANSFER_TOKEN"},
        {"grant_type", "refresh_token"},
        {"refresh_token", refresh_token},
    };

    httplib::Headers headers = {{"User-Agent", _userAgent}};

    httplib::Client cli("https://login.live.com");
    httplib::Result res = cli.Post("/oauth20_token.srf", headers, httplib::detail::params_to_query_str(payload), "application/x-www-form-urlencoded");
    if (res && res->status == 200) {
#ifdef _DEBUG
        std::cout << "MSAL Token Response: " << res->body << "\n\n";
#endif
        out_token = nlohmann::json::parse(res->body).get<XAL_UserToken>();
    } else {
        throw std::runtime_error("Failed to get MSAL token from refresh token");
    }

    LOG_INFO("XAL Token 获取成功.");
}

void MSAL::refreshOAtuh2Token(const std::string& refresh_token, MSAL_OAuth2Token& out_token) {

    httplib::Params payload = {
        {"client_id", _clientId},
        {"scope", "xboxlive.signin openid profile offline_access"},
        {"grant_type", "refresh_token"},
        {"refresh_token", refresh_token},
    };

    httplib::Headers headers = {{"User-Agent", _userAgent}, {"Cache-Control", "no-store, must-revalidate, no-cache"}};

    httplib::Client cli("https://login.microsoftonline.com");
    httplib::Result res =
        cli.Post("/consumers/oauth2/v2.0/token", headers, httplib::detail::params_to_query_str(payload), "application/x-www-form-urlencoded");
    if (res && res->status == 200) {
#ifdef _DEBUG
        std::cout << "MSAL Token Refresh Response: " << res->body << "\n\n";
#endif
        out_token = nlohmann::json::parse(res->body).get<MSAL_OAuth2Token>();
        out_token.updateExpiry();
    } else {
        throw std::runtime_error("Failed to refresh MSAL token from refresh token");
    }

    LOG_INFO("OAuth2 Token 刷新成功.");
}
