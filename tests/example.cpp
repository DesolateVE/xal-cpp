#include <chrono>
#include <fstream>
#include <httplib.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <regex>
#include <thread>
#include <unordered_map>
#include <windows.h>

#include "utils/helper.hpp"
#include "webscript/WebView2Automation.hpp"
#include "webscript/webview2_window.hpp"
#include "xal/xal.hpp"

using json = nlohmann::json;

// GetCredentialType 响应结构体定义
namespace CredentialType {

// 可用于一次性登录的验证方式
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

// 凭据信息
struct Credentials {
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

// 完整响应结构
struct Response {
    std::string Username;                    // 用户名
    std::string Display;                     // 显示名称
    std::string Location;                    // 位置信息
    int         IfExistsResult;              // 账户存在状态：0=存在, 1=不存在, 2=需要重定向, 等
    bool        AliasDisabledForLogin;       // 别名是否禁用登录
    Credentials Credentials;                 // 凭据信息
    bool        FullPasswordResetExperience; // 是否为完整密码重置体验
    std::string FlowToken;                   // 新的流程令牌（用于下一步）
    int         ThrottleStatus;              // 限流状态：0=正常, 1=被限流
    std::string apiCanary;                   // API Canary 令牌
};

// nlohmann::json 自动序列化支持
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
    Credentials,
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
    Response,
    Username,
    Display,
    Location,
    IfExistsResult,
    AliasDisabledForLogin,
    Credentials,
    FullPasswordResetExperience,
    FlowToken,
    ThrottleStatus,
    apiCanary
)
} // namespace CredentialType

int main() {

    XAL         xal("tokens.json");
    std::string loginUrl = xal.getLoginUri();

    std::cout << "Login URL: " << loginUrl << "\n";

    auto parse_query_params = [](const std::string& url) {
        std::unordered_map<std::string, std::string> params;
        auto                                         qpos = url.find('?');
        if (qpos == std::string::npos)
            return params;
        std::string query = url.substr(qpos + 1);
        size_t      pos   = 0;
        while (pos < query.size()) {
            auto amp = query.find('&', pos);
            if (amp == std::string::npos)
                amp = query.size();
            auto eq = query.find('=', pos);
            if (eq != std::string::npos && eq < amp) {
                auto k    = query.substr(pos, eq - pos);
                auto v    = query.substr(eq + 1, amp - (eq + 1));
                params[k] = v; // 保留原始值（未解码），便于你后续处理
            }
            pos = amp + 1;
        }
        return params;
    };

    auto rx_find = [](const std::string& text, const std::string& pattern) {
        std::smatch m;
        std::regex  re(pattern, std::regex::icase | std::regex::optimize);
        if (std::regex_search(text, m, re) && m.size() > 1)
            return m[1].str();
        return std::string();
    };

    size_t      schemeEnd = loginUrl.find("://");
    size_t      pathStart = loginUrl.find("/", schemeEnd + 3);
    std::string host      = loginUrl.substr(0, pathStart);
    std::string path      = loginUrl.substr(pathStart);

    httplib::Client cli(host);
    cli.set_follow_location(true); // 允许重定向
    auto res = cli.Get(path);

    if (!res || res->status != 200) {
        std::cerr << "Failed to get page, error: " << (res ? std::to_string(res->status) : "connection failed") << std::endl;
        return -1;
    }

    // 解析 URL 查询参数（client_id、redirect_uri、scope 等）
    auto q = parse_query_params(loginUrl);

    // 从 HTML 中提取关键字段
    const auto& html = res->body;

    // 保存原始 HTML 到文件便于诊断
    std::ofstream htmlFile("login_response.html");
    htmlFile << html;
    htmlFile.close();
    std::cout << "\n[DEBUG] HTML saved to login_response.html\n";
    std::cout << "[DEBUG] HTML length: " << html.length() << " bytes\n\n";

    // 从 HTML 中提取 ServerData 这个 JSON 对象
    std::regex  serverDataRegex(R"MSFT(var\s+ServerData\s*=\s*(\{.*?\});)MSFT", std::regex::icase);
    std::smatch match;
    std::string jsonStr;

    if (std::regex_search(html, match, serverDataRegex) && match.size() > 1) {
        jsonStr = match[1].str();
    } else {
        std::cerr << "Failed to extract ServerData JSON from HTML\n";
        return -1;
    }

    // 解析 JSON
    using json = nlohmann::json;
    json data;
    try {
        data = json::parse(jsonStr);
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse ServerData JSON: " << e.what() << "\n";
        return -1;
    }

    // 提取关键字段
    std::string ppftHtml             = data.value("sFTTag", "");
    std::string urlPost              = data.value("urlPost", "");
    std::string uaid                 = data.value("sUnauthSessionID", "");
    std::string sNGCNonce            = data.value("sNGCNonce", "");
    std::string correlationId        = data.value("correlationId", "");
    std::string urlGetCredentialType = data.value("urlGetCredentialType", "");

    // 从 sFTTag 的 input 元素中提取 PPFT token 的实际值
    std::regex  ppftValueRegex(R"MSFT(value=["']([^"']+)["'])MSFT");
    std::smatch ppftMatch;
    std::string ppft = "";
    if (std::regex_search(ppftHtml, ppftMatch, ppftValueRegex) && ppftMatch.size() > 1) {
        ppft = ppftMatch[1].str();
    }

    // 打印关键信息，供后续操作
    std::cout << "\n=== Parsed OAuth Params (from loginUrl) ===\n";
    auto print_or_dash = [](const std::unordered_map<std::string, std::string>& m, const char* k) {
        auto it = m.find(k);
        return it == m.end() ? std::string("-") : it->second;
    };
    std::cout << "client_id       : " << print_or_dash(q, "client_id") << "\n";
    std::cout << "redirect_uri    : " << print_or_dash(q, "redirect_uri") << "\n";
    std::cout << "scope           : " << print_or_dash(q, "scope") << "\n";
    std::cout << "response_type   : " << print_or_dash(q, "response_type") << "\n";
    std::cout << "state           : " << print_or_dash(q, "state") << "\n";
    std::cout << "\n=== Extracted Tokens (from HTML) ===\n";
    std::cout << "PPFT            : " << (ppft.empty() ? "-" : ppft) << "\n";
    std::cout << "urlPost         : " << (urlPost.empty() ? "-" : urlPost) << "\n";
    std::cout << "uaid            : " << (uaid.empty() ? "-" : uaid) << "\n";
    std::cout << "sNGCNonce       : " << (sNGCNonce.empty() ? "-" : sNGCNonce) << "\n";
    std::cout << "correlationId   : " << (correlationId.empty() ? "-" : correlationId) << "\n";
    std::cout << "urlGetCredentialType: " << (urlGetCredentialType.empty() ? "-" : urlGetCredentialType) << "\n";

    // 构造 GetCredentialType 请求
    json credTypePayload = {
        {"username", "entergulungi@hotmail.com"}, // 替换为实际邮箱
        {"flowToken", ppft},
        {"uaid", uaid},
        {"isReactLoginRequest", true},
        {"isFidoSupported", false},
        {"isRemoteNGCSupported", true},
        {"checkPhones", false},
        {"country", ""},
        {"federationFlags", 11},
        {"forceotclogin", false},
        {"isCookieBannerShown", false},
        {"isExternalFederationDisallowed", false},
        {"isFederationDisabled", false},
        {"isOtherIdpSupported", false},
        {"isSignup", false},
        {"originalRequest", ""},
        {"otclogindisallowed", false},
        {"isRemoteConnectSupported", false}
    };

    std::string credTypeBody = credTypePayload.dump();

    // 从初始请求中保存 Cookie
    std::string cookies = "";
    if (res->has_header("Set-Cookie")) {
        auto cookie_headers = res->get_header_value("Set-Cookie");
        cookies             = cookie_headers;
    }

    // 构造完整的 Headers,模拟真实浏览器
    httplib::Headers headers = {
        {"Accept", "application/json"},
        {"Accept-Encoding", "gzip, deflate, br, zstd"},
        {"Accept-Language", "zh-CN,zh;q=0.9"},
        {"Cache-Control", "no-cache"},
        {"Client-Request-Id", correlationId},
        {"Connection", "keep-alive"},
        {"Content-Type", "application/json; charset=utf-8"},
        {"Hpgrequestid", correlationId},
        {"Origin", "https://login.live.com"},
        {"Pragma", "no-cache"},
        {"Referer", loginUrl},
        {"Sec-Fetch-Dest", "empty"},
        {"Sec-Fetch-Mode", "cors"},
        {"Sec-Fetch-Site", "same-origin"},
        {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36"},
        {"sec-ch-ua", "\"Google Chrome\";v=\"131\", \"Chromium\";v=\"131\", \"Not_A Brand\";v=\"24\""},
        {"sec-ch-ua-mobile", "?0"},
        {"sec-ch-ua-platform", "\"Windows\""}
    };

    // 如果有 apiCanary 则添加
    std::string apiCanary = data.value("apiCanary", "");
    if (!apiCanary.empty()) {
        headers.emplace("Canary", apiCanary);
    }

    // 添加 Cookie (如果有的话)
    if (!cookies.empty()) {
        headers.emplace("Cookie", cookies);
    }

    // 解析 urlGetCredentialType 获取完整 URL 和路径
    std::string credTypeFullUrl = urlGetCredentialType;
    if (credTypeFullUrl.find("http") != 0) {
        credTypeFullUrl = "https://login.live.com" + urlGetCredentialType;
    }

    size_t      credSchemeEnd = credTypeFullUrl.find("://");
    size_t      credPathStart = credTypeFullUrl.find("/", credSchemeEnd + 3);
    std::string credPath      = credTypeFullUrl.substr(credPathStart);

    std::cout << "\n=== Sending GetCredentialType Request ===\n";
    std::cout << "URL: " << credTypeFullUrl << "\n";
    std::cout << "Payload: " << credTypeBody << "\n\n";

    auto credRes = cli.Post(credPath, headers, credTypeBody, "application/json");

    if (credRes && credRes->status == 200) {
        std::cout << "\n=== GetCredentialType Response ===\n";
        std::cout << "Status: " << credRes->status << "\n";
        std::cout << "Body: " << credRes->body << "\n\n";

        try {
            // 解析返回的 JSON
            auto                     credJson     = json::parse(credRes->body);
            CredentialType::Response credResponse = credJson.get<CredentialType::Response>();

            std::cout << "data:" << credResponse.Credentials.OtcLoginEligibleProofs[0].data << "\n";

        } catch (const std::exception& e) { std::cerr << "Failed to parse GetCredentialType response: " << e.what() << "\n"; }
    } else {
        std::cerr << "\n=== GetCredentialType Failed ===\n";
        std::cerr << "Status: " << (credRes ? std::to_string(credRes->status) : "no response") << "\n";
        if (credRes) {
            std::cerr << "Body: " << credRes->body << "\n";
        }
    }

    return 0;
}
