#pragma once
#include <chrono>
#include <ctime>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

struct UserToken {
    std::string token_type;
    uint64_t    expires_in;
    std::string scope;
    std::string access_token;
    std::string refresh_token;
    std::string user_id;
    uint64_t    expires_at;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
        UserToken, token_type, expires_in, scope, access_token, refresh_token, user_id, expires_at)
    void updateExpiry();
    bool isExpired() const;
};

// ===== Sisu token models (from sisu/authenticate response) =====

struct SisuTitleDisplayClaims {
    struct Xti {
        std::string tid;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Xti, tid)
    };
    Xti xti;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(SisuTitleDisplayClaims, xti)
};

struct SisuTitleToken {
    SisuTitleDisplayClaims DisplayClaims;
    std::string            IssueInstant;
    std::string            NotAfter;
    std::string            Token;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(SisuTitleToken, DisplayClaims, IssueInstant, NotAfter, Token)
};

struct SisuUserDisplayClaims {
    struct Xui {
        std::string uhs;
        std::string gtg;
        std::string xid;
        std::string mgt;
        std::string mgs;
        std::string umg;
        std::string agg;
        std::string usr;
        std::string prv;
        std::string ugc;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Xui, uhs, gtg, xid, mgt, mgs, umg, agg, usr, prv, ugc)
    };
    std::vector<Xui> xui;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(SisuUserDisplayClaims, xui)
};

struct SisuUserToken {
    SisuUserDisplayClaims DisplayClaims;
    std::string           IssueInstant;
    std::string           NotAfter;
    std::string           Token;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(SisuUserToken, DisplayClaims, IssueInstant, NotAfter, Token)
};

struct UcsMigrationResponse {
    std::vector<std::string> gcsConsentsToOverride;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(UcsMigrationResponse, gcsConsentsToOverride)
};

struct SisuToken {
    std::string          DeviceToken;
    SisuTitleToken       TitleToken;
    SisuUserToken        UserToken;           // user token
    SisuUserToken        AuthorizationToken;  // authz token with xui
    std::string          WebPage;
    std::string          Sandbox;
    bool                 UseModernGamertag;
    UcsMigrationResponse UcsMigrationResponse;
    std::string          Flow;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(SisuToken,
                                   DeviceToken,
                                   TitleToken,
                                   UserToken,
                                   AuthorizationToken,
                                   WebPage,
                                   Sandbox,
                                   UseModernGamertag,
                                   UcsMigrationResponse,
                                   Flow)

    bool isExpired();
};

struct XstsToken {
    // 对应 XSTS 授权返回的 DisplayClaims.xui 列表
    struct DisplayClaimsType {
        struct Xui {
            std::string gtg;  // Gamertag
            std::string xid;  // Xbox ID
            std::string uhs;  // User Hash
            std::string agg;  // Age Group (Adult/Teen/...)
            std::string usr;  // 用户特征串 (示例: "195 234")
            std::string prv;  // 权限列表 (空格分隔数字)
            std::string ugc;  // UGC 相关权限串
            NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Xui, gtg, xid, uhs, agg, usr, prv, ugc)
        };
        std::vector<Xui> xui;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(DisplayClaimsType, xui)
    };

    std::string        IssueInstant;    // 令牌签发时间 ISO8601
    std::string        NotAfter;        // 过期时间 ISO8601
    std::string        Token;           // XSTS JWT (加密包装)
    DisplayClaimsType  DisplayClaims;   // 里面的 xui 数组
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(XstsToken, IssueInstant, NotAfter, Token, DisplayClaims)
    bool isExpired() const;
    uint64_t secondsUntilExpiry() const;

  private:
    // 简单解析形如 2025-11-26T08:32:10.5118384Z 的时间（忽略毫秒部分）
    static std::chrono::system_clock::time_point parse_iso8601_utc(const std::string& s);
};