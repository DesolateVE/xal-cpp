#include "msal_login.hpp"

#include <cpr/cpr.h>
#include <format>
#include <nlohmann/json.hpp>
#include <regex>
#include <stdexcept>

#include "utils/helper.hpp"
#include "utils/logger.hpp"
#include "utils/regular.hpp"

static const cpr::Header defaultHeaders = {
    {"Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6"},
    {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36 Edg/144.0.0.0"},
    {"sec-ch-ua", R"xx("Not(A:Brand";v="8", "Chromium";v="144", "Microsoft Edge";v="144")xx"},
    {"sec-ch-ua-mobile", "?0"},
    {"sec-ch-ua-platform", "Windows"},
    {"sec-ch-ua-platform-version", "19.0.0"}
};

MSALLogin::MSALLogin() { session = new cpr::Session(); }

MSALLogin::~MSALLogin() { delete session; }

void MSALLogin::MSALEasyLogin(const std::string& url, const std::string& code, const std::string& email, const std::string& password) {
    session->SetHeader(defaultHeaders);
    session->SetOption(cpr::Cookie{"", ""});

    Credential result = GetCodeAuth(url);
    result            = PostCodeAuth(result, code);
    result            = PostAccountAuth(result, email, password);
    PostLoginFinish(result);
}

auto MSALLogin::GetCodeAuth(const std::string& url) -> Credential {
    session->SetUrl(url);
    auto result = session->Get();

    if (result.error)
        throw std::runtime_error("Failed to get code auth HTML: " + result.error.message);

    // 使用正则找到 var ServerData = {...};
    std::regex  serverDataRegex(R"xx(var\s+ServerData\s*=\s*(\{.*?\});)xx");
    std::smatch match;
    if (!std::regex_search(result.text, match, serverDataRegex))
        throw std::runtime_error("ServerData not found in response HTML.");

    // 解析 JSON
    auto jsonData = nlohmann::json::parse(match[1].str());

    // 提取需要的信息
    std::string sFT     = jsonData["sFT"];
    std::string urlPost = jsonData["urlPost"];

    LOG_INFO("PPFT and urlPost extracted successfully.");
    LOG_INFO(std::format("\t- urlPost: {}", urlPost));
    LOG_INFO(std::format("\t- sFT: {}", sFT));

    return {sFT, urlPost};
}

auto MSALLogin::PostCodeAuth(const Credential& credential, const std::string& code) -> Credential {
    session->SetUrl(credential.urlPost);
    session->SetPayload({{"PPFT", credential.ppft}, {"otc", code}, {"i19", std::to_string(std::rand_int(0, 99999))}});
    auto result = session->Post();
    session->SetPayload({});
    if (result.error)
        throw std::runtime_error("Failed to post code auth: " + result.error.message);

    // 使用正则找到 var ServerData = {...};
    std::regex  serverDataRegex(R"xx(var\s+ServerData\s*=\s*(\{.*?\});)xx");
    std::smatch match;
    if (!std::regex_search(result.text, match, serverDataRegex))
        throw std::runtime_error("ServerData not found in response HTML.");

    // 解析 JSON
    auto jsonData = nlohmann::json::parse(match[1].str());

    // 提取需要的信息
    std::string sFTTag      = jsonData["sFTTag"];
    std::string urlPost     = jsonData["urlPost"].empty() ? jsonData["urlPostMsa"] : jsonData["urlPost"];
    std::string sRandomBlob = jsonData["sRandomBlob"];

    // 继续提取 sFTTag 中的 value
    // <input type=\"hidden\" name=\"PPFT\" id=\"i0327\"
    // value=\"-Dlu2ttFdD9CTusoGEedYpUrLS8h5CkCnaUl2sVSngcnW2yyAvadOy72yM1ZRil0WRvvAgVzwyP1AaUdVZgCCYM!ekkos4wPYU0JupZzzKiBZ7Gx1OC1iQTRTUoqNupjYDPR2bmIk9BKGVEGYKfDowdJxGxx6HhukoncwzySN*VF!!JmFDjiYAcfsX7fYZTjK61v3BsUizPK9qe6BasHFLW9Tqj1bIeeuZBqMl0W8HnSi5FM3rFw4RlfJtJVvA*Bzyj6RMQjs*hTnIHrDTE8!Ykz7fQmGSGMhDelO6X*rnohB\"/>
    // 使用正则找到 value="..."
    std::regex  valueRegex(R"xx(value=["'](.*?)["'])xx");
    std::smatch valueMatch;
    if (!std::regex_search(sFTTag, valueMatch, valueRegex))
        throw std::runtime_error("PPFT value not found in sFTTag.");

    std::string ppftValue = valueMatch[1].str();

    LOG_INFO("PPFT and urlPost extracted successfully after posting code.");
    LOG_INFO(std::format("\t- urlPost: {}", urlPost));
    LOG_INFO(std::format("\t- PPFT: {}", ppftValue));
    LOG_INFO(std::format("\t- sRandomBlob: {}", sRandomBlob));

    return {ppftValue, urlPost, sRandomBlob};
}

auto MSALLogin::PostAccountAuth(const Credential& credential, const std::string& email, const std::string& password) -> Credential {
    session->SetUrl(credential.urlPost);
    session->SetPayload(
        {{"ps", "2"},
         {"PPFT", credential.ppft},
         {"PPSX", credential.ppsx},
         {"NewUser", "1"},
         {"fspost", "0"},
         {"i21", "0"},
         {"CookieDisclosure", "0"},
         {"IsFidoSupported", "0"},
         {"isSignupPost", "0"},
         {"isRecoveryAttemptPost", "0"},
         {"i13", "0"},
         {"login", email},
         {"loginfmt", email},
         {"type", "11"},
         {"LoginOptions", "3"},
         {"cpr", "0"},
         {"passwd", password}}
    );
    auto result = session->Post();
    session->SetPayload({});

    if (result.error)
        throw std::runtime_error("Failed to post account auth: " + result.error.message);

    // 使用正则找到 var ServerData = {...};
    std::regex  serverDataRegex(R"xx(var\s+ServerData\s*=\s*(\{.*?\});)xx");
    std::smatch match;
    if (!std::regex_search(result.text, match, serverDataRegex)) {
        // 说明登陆方式走了另外一个分支
        auto actionUrl = Regular::extractActionUrl(result.text);

        // 提取 actionUrl 中的参数
        auto params  = Regular::extractUrlParameters(actionUrl);
        auto jumpUrl = Regular::urlDecode(params["ru"]);

        // 继续访问 jumpUrl 获取最终的 token
        session->SetUrl(jumpUrl);
        auto jumpResult = session->Get();

        if (jumpResult.error)
            throw std::runtime_error("Failed to get jump URL: " + jumpResult.error.message);

        // 使用正则找到 var ServerData = {...};
        if (!std::regex_search(jumpResult.text, match, serverDataRegex))
            throw std::runtime_error("ServerData not found in jump URL response HTML.");
    }

    // 解析 JSON
    auto        jsonData = nlohmann::json::parse(match[1].str());
    std::string urlPost  = jsonData["urlPost"];
    std::string sFT      = jsonData["sFT"];

    LOG_INFO("PPFT and urlPost extracted successfully after posting account.");
    LOG_INFO(std::format("\t- urlPost: {}", urlPost));
    LOG_INFO(std::format("\t- sFT: {}", sFT));

    return {sFT, urlPost};
}

auto MSALLogin::PostLoginFinish(const Credential& credential) -> void {
    session->SetUrl(credential.urlPost);
    session->SetPayload({{"PPFT", credential.ppft}, {"LoginOptions", "3"}, {"type", "28"}});
    auto result = session->Post();
    session->SetPayload({});
    if (result.error)
        throw std::runtime_error("Failed to post login finish: " + result.error.message);

    LOG_INFO("🍭 Login finished successfully.");
}
