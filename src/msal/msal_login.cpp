#include "msal_login.hpp"

#include <cpr/cpr.h>
#include <format>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <vector>

#include "cpr/curl_container.h"
#include "cpr/response.h"
#include "utils/helper.hpp"
#include "utils/logger.hpp"
#include "utils/regular.hpp"

static const cpr::Header defaultHeaders = {
    {"accept-language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6"},
    {"user-agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/145.0.0.0 Safari/537.36 Edg/145.0.0.0"},
    {"sec-ch-ua", R"xx("Not(A:Brand";v="8", "Chromium";v="144", "Microsoft Edge";v="144")xx"},
    {"sec-ch-ua-mobile", "?0"},
    {"sec-ch-ua-platform", "Windows"},
    {"sec-ch-ua-platform-version", "19.0.0"}
};

MSALLogin::MSALLogin() { session = new cpr::Session(); }

MSALLogin::~MSALLogin() { delete session; }

void MSALLogin::MSALEasyLogin(const std::string& url, const std::string& code, const std::string& email, const std::string& password) {
    LOG_INFO("[MSALEasyLogin] 开始登录流程，账号: {}", email);
    session->SetHeader(defaultHeaders);
    session->SetOption(cpr::Cookie{"", ""});
    session->SetRedirect(cpr::Redirect{true});
    session->SetTimeout(cpr::Timeout{20000});

    std::string sFT;
    std::string urlPost;
    LOG_INFO("[MSALEasyLogin] Step 1: 获取登录页面及认证令牌...");
    GetCodeAuth(url, sFT, urlPost);

    std::string sRandomBlob;
    std::string newSFTTag;
    LOG_INFO("[MSALEasyLogin] Step 2: 提交验证码...");
    PostCodeAuth(urlPost, sFT, code, urlPost, sRandomBlob, newSFTTag);

    LOG_INFO("[MSALEasyLogin] Step 3: 提交账号密码...");
    PostAccountAuth(urlPost, newSFTTag, sRandomBlob, email, password);
    LOG_INFO("[MSALEasyLogin] 登录流程全部完成.");
}

auto MSALLogin::GetCodeAuth(const std::string& url, std::string& sFT, std::string& urlPost) -> void {
    LOG_INFO("[GetCodeAuth] 正在请求登录页面: {}", url);
    session->SetUrl(url);
    auto result = session->Get();

    if (result.error)
        throw std::runtime_error("Failed to get code auth HTML: " + result.error.message);

    auto jsonData = nlohmann::json::parse(Regular::extractServerDataJson(result.text));

    if (!jsonData.contains("sFT"))
        throw std::runtime_error("sFT not found in ServerData JSON.");

    if (!jsonData.contains("urlPost"))
        throw std::runtime_error("urlPost not found in ServerData JSON.");

    sFT     = jsonData["sFT"].get<std::string>();
    urlPost = jsonData["urlPost"].get<std::string>();

    LOG_INFO("[GetCodeAuth] 1. 登录界面获取成功，sFT 和 urlPost 提取成功，下一步 urlPost: {}", urlPost);
}

auto MSALLogin::PostCodeAuth(
    const std::string& url,
    const std::string& sFT,
    const std::string& code,
    std::string&       urlPost,
    std::string&       sRandomBlob,
    std::string&       newSFTTag
) -> void {
    LOG_INFO("[PostCodeAuth] 正在提交验证码 (otc: {}) 到: {}", code, url);
    session->SetUrl(url);
    session->SetPayload({{"PPFT", sFT}, {"otc", code}, {"i19", std::to_string(std::rand_int(0, 99999))}});
    auto result = session->Post();

    if (result.error)
        throw std::runtime_error("Failed to post code auth: " + result.error.message);

    auto jsonData = nlohmann::json::parse(Regular::extractServerDataJson(result.text));

    if (!jsonData.contains("sFTTag"))
        throw std::runtime_error("sFTTag not found in ServerData JSON.");

    if (!jsonData.contains("urlPost"))
        throw std::runtime_error("urlPost not found in ServerData JSON.");

    if (!jsonData.contains("sRandomBlob"))
        throw std::runtime_error("sRandomBlob not found in ServerData JSON.");

    urlPost     = jsonData["urlPost"];
    sRandomBlob = jsonData["sRandomBlob"];
    newSFTTag   = Regular::extractInputNameValue(jsonData["sFTTag"])["PPFT"];

    LOG_INFO(
        "[PostCodeAuth] 2. 验证码验证成功，新的 sFTTag、sRandomBlob 和 urlPost 提取成功，下一步 urlPost: {}",
        jsonData["urlPost"].get<std::string>()
    );
}

auto MSALLogin::SubmitFinalAuth(const std::string& urlPost, const std::string& sftTag) -> void {
    LOG_INFO("[SubmitFinalAuth] 正在提交最终授权到: {}", urlPost);
    session->SetUrl(urlPost);
    session->SetPayload({{"PPFT", sftTag}, {"LoginOptions", "3"}, {"type", "28"}});
    auto result = session->Post();
    session->SetPayload({});
    if (result.error)
        throw std::runtime_error("Failed to submit final auth: " + result.error.message);
    LOG_INFO("[SubmitFinalAuth] 最终授权提交完成，HTTP 状态码: {}", result.status_code);
}

auto MSALLogin::HandleProofsRemind(const Regular::FormData& formData, const std::string& email, const std::string& password) -> void {
    LOG_INFO("[HandleProofsRemind] 检测到安全信息提醒页面，正在跳过...");
    // 将表单原样提交，告知微软账号信息无误
    session->SetUrl(formData.action);
    std::vector<cpr::Pair> payloadVec;
    for (const auto& [key, value] : formData.inputs)
        payloadVec.emplace_back(key, value);
    session->SetPayload(cpr::Payload(payloadVec.begin(), payloadVec.end()));
    LOG_INFO("[HandleProofsRemind] 正在提交提醒跳过表单到: {}", formData.action);
    auto remindResult = session->Post();
    if (remindResult.error)
        throw std::runtime_error("Failed to post remind form: " + remindResult.error.message);

    // 再次提交证明新鲜度检查
    LOG_INFO("[HandleProofsRemind] 正在提交证明新鲜度检查...");
    auto canaryValue = Regular::extractInputNameValue(remindResult.text)["canary"];
    session->SetPayload({{"ProofFreshnessAction", "1"}, {"canary", canaryValue}});
    auto proofResult = session->Post();
    if (proofResult.error)
        throw std::runtime_error("Failed to post proof freshness form: " + proofResult.error.message);

    // Object moved 重定向，提取跳转链接并访问
    auto jmpUrl = Regular::extractTagAttribute(proofResult.text, "a", "href");
    LOG_INFO("[HandleProofsRemind] 正在跟随重定向到: {}", jmpUrl);
    session->SetUrl(jmpUrl);
    session->SetPayload({});
    auto jmpResult = session->Get();
    if (jmpResult.error)
        throw std::runtime_error("Failed to get proof freshness redirect URL: " + jmpResult.error.message);

    auto newFormData = Regular::extractFormData(Regular::extractHtmlBody(jmpResult.text));
    auto params      = Regular::extractUrlParameters(newFormData.action);
    if (!params.contains("ru"))
        throw std::runtime_error("ru parameter not found in proofs remind redirect form action URL.");

    auto redirectedLink = Regular::urlDecode(params["ru"]);
    LOG_INFO("[HandleProofsRemind] 正在访问最终重定向链接: {}", redirectedLink);
    session->SetUrl(redirectedLink);
    session->SetBody({});
    auto redirectResult = session->Get();
    if (redirectResult.error)
        throw std::runtime_error("Failed to get proofs remind redirect URL: " + redirectResult.error.message);

    auto newServerDataJson = Regular::extractServerDataJson(redirectResult.text);
    if (newServerDataJson.empty())
        throw std::runtime_error("ServerData not found in proofs remind redirect response HTML.");

    LOG_INFO("[HandleProofsRemind] 安全信息提醒跳过完成，准备提交最终授权.");
    auto newServerData = nlohmann::json::parse(newServerDataJson);
    SubmitFinalAuth(newServerData["urlPost"].get<std::string>(), newServerData["sFT"].get<std::string>());
}

auto MSALLogin::HandlePasskeyInterrupt(const Regular::FormData& formData) -> void {
    LOG_INFO("[HandlePasskeyInterrupt] 检测到通行密钥申请页面，正在跳过...");
    // 跳过通行密钥申请：提取 ru 参数并重定向
    auto redirectedLink = Regular::urlDecode(Regular::extractUrlParameters(formData.action)["ru"]);
    LOG_INFO("[HandlePasskeyInterrupt] 正在跟随重定向到: {}", redirectedLink);
    session->SetUrl(redirectedLink);
    session->SetBody({});
    auto passkeyResult = session->Get();
    if (passkeyResult.error)
        throw std::runtime_error("Failed to get passkey redirect URL: " + passkeyResult.error.message);

    auto newServerDataJson = Regular::extractServerDataJson(passkeyResult.text);
    if (newServerDataJson.empty())
        throw std::runtime_error("ServerData not found in passkey redirect response HTML.");

    LOG_INFO("[HandlePasskeyInterrupt] 通行密钥页面跳过完成，准备提交最终授权.");
    auto newServerData = nlohmann::json::parse(newServerDataJson);
    SubmitFinalAuth(newServerData["urlPost"].get<std::string>(), newServerData["sFT"].get<std::string>());
}

auto MSALLogin::HandleContinuePage(const std::string& html, const std::string& email, const std::string& password) -> void {
    auto formData = Regular::extractFormData(Regular::extractHtmlBody(html));
    LOG_INFO("[HandleContinuePage] 检测到继续页面，form action: {}", formData.action);

    if (formData.action.starts_with("https://account.live.com/proofs/remind")) {
        LOG_INFO("[HandleContinuePage] 进入安全信息提醒处理流程.");
        HandleProofsRemind(formData, email, password);
    } else if (formData.action.starts_with("https://account.live.com/interrupt/passkey")) {
        LOG_INFO("[HandleContinuePage] 进入通行密钥跳过流程.");
        HandlePasskeyInterrupt(formData);
    } else if (formData.action.starts_with("https://account.live.com/identity/confirm")) {
        throw std::runtime_error("需要进行邮箱或电话验证，需要手工处理。");
    } else {
        throw std::runtime_error("未处理的继续页面 form action: " + formData.action);
    }
}

auto MSALLogin::PostAccountAuth(
    const std::string& url,
    const std::string& sFT,
    const std::string& sRandomBlob,
    const std::string& email,
    const std::string& password
) -> void {
    LOG_INFO("[PostAccountAuth] 正在提交账号密码到: {}", url);
    session->SetUrl(url);
    session->SetPayload({
        {"ps", "2"},
        {"PPFT", sFT},
        {"PPSX", sRandomBlob},
        {"NewUser", "1"},
        {"fspost", "0"},
        {"i21", "0"},
        {"CookieDisclosure", "0"},
        {"IsFidoSupported", "1"},
        {"isSignupPost", "0"},
        {"isRecoveryAttemptPost", "0"},
        {"i13", "0"},
        {"login", email},
        {"loginfmt", email},
        {"type", "11"},
        {"LoginOptions", "3"},
        {"cpr", "0"},
        {"passwd", password},
    });
    auto result = session->Post();
    if (result.error)
        throw std::runtime_error("Failed to post account auth: " + result.error.message);

    auto title = Regular::extractTagContent(result.text, "title");
    if (title.empty())
        throw std::runtime_error("Title tag not found in account auth response HTML.");

    LOG_INFO("[PostAccountAuth] 收到响应页面标题: {}", title);

    if (title == "Microsoft 帐户") {
        LOG_INFO("[PostAccountAuth] 3. 账号密码验证成功，正在提交最终授权...");
        auto serverData = nlohmann::json::parse(Regular::extractServerDataJson(result.text));
        SubmitFinalAuth(serverData["urlPost"].get<std::string>(), serverData["sFT"].get<std::string>());
    } else if (title == "登录你的 Microsoft 帐户") {
        // 大概率是密码错误
        auto serverData = nlohmann::json::parse(Regular::extractServerDataJson(result.text));
        if (serverData.contains("sErrTxt") && serverData.contains("sErrorCode"))
            throw std::runtime_error(
                std::format(
                    "登录失败，错误代码: {}, 错误信息: {}",
                    serverData["sErrorCode"].get<std::string>(),
                    serverData["sErrTxt"].get<std::string>()
                )
            );
        throw std::runtime_error("登录失败，未知错误，且未能从服务器数据中提取错误信息。");
    } else if (title == "继续") {
        HandleContinuePage(result.text, email, password);
    } else {
        throw std::runtime_error("未处理的登录结果页面标题: " + title);
    }
}
