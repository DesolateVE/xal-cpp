#pragma once
#include <chrono>
#include <ctime>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

#include "../mslogin_export.hpp"

// 补充 std::optional 的参数依赖查找序列化以兼容当前环境
NLOHMANN_JSON_NAMESPACE_BEGIN
template <typename T> struct adl_serializer<std::optional<T>> {
    static void to_json(json& j, const std::optional<T>& opt) {
        if (opt)
            j = *opt;
        else
            j = nullptr;
    }
    static void from_json(const json& j, std::optional<T>& opt) {
        if (j.is_null())
            opt.reset();
        else
            opt = j.get<T>();
    }
};
NLOHMANN_JSON_NAMESPACE_END

// ===== 设备代码认证流程 =====
struct MSLOGIN_API MSAL_IDeviceCodeAuth {
    std::string user_code;
    std::string device_code;
    std::string verification_uri;
    int         expires_in;
    int         interval;
    std::string message;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(MSAL_IDeviceCodeAuth, user_code, device_code, verification_uri, expires_in, interval, message)
};

struct MSLOGIN_API MSAL_OAuth2Token {
    std::string token_type;
    std::string scope;
    int         expires_in;
    int         ext_expires_in;
    std::string access_token;
    std::string refresh_token;
    std::string id_token;
    uint64_t    expires_at;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
        MSAL_OAuth2Token,
        token_type,
        scope,
        expires_in,
        ext_expires_in,
        access_token,
        refresh_token,
        id_token,
        expires_at
    )
    void updateExpiry();
    bool isExpired() const;
};

// ===== 用户令牌 (Microsoft 账户 OAuth 令牌) =====
struct MSLOGIN_API XAL_UserToken {
    std::string token_type;
    uint64_t    expires_in;
    std::string scope;
    std::string access_token;
    std::string refresh_token;
    std::string user_id;
    uint64_t    expires_at;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(XAL_UserToken, token_type, expires_in, scope, access_token, refresh_token, user_id, expires_at)
    void updateExpiry();
    bool isExpired() const;
};

struct MSLOGIN_API IMsalToken {
    std::string lpt;
    std::string refresh_token;
    std::string user_id;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(IMsalToken, lpt, refresh_token, user_id)
};

// ===== Sisu 令牌模型 (来自 sisu/authenticate 响应) =====
struct MSLOGIN_API SisuToken {
    struct TitleDisplayClaims {
        struct Xti {
            std::string tid;
            NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Xti, tid)
        };
        Xti xti;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(TitleDisplayClaims, xti)
    };

    struct _TitleToken {
        TitleDisplayClaims DisplayClaims;
        std::string        IssueInstant;
        std::string        NotAfter;
        std::string        Token;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(_TitleToken, DisplayClaims, IssueInstant, NotAfter, Token)
    };

    struct UserDisplayClaims {
        struct Xui {
            std::string agg;
            std::string gtg;
            std::string mgs;
            std::string mgt;
            std::string prv;
            std::string ugc;
            std::string uhs;
            std::string umg;
            std::string usr;
            std::string xid;
            NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Xui, agg, gtg, mgs, mgt, prv, ugc, uhs, umg, usr, xid)
        };
        std::vector<Xui> xui;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(UserDisplayClaims, xui)
    };

    struct _UserToken {
        UserDisplayClaims DisplayClaims;
        std::string       IssueInstant;
        std::string       NotAfter;
        std::string       Token;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(_UserToken, DisplayClaims, IssueInstant, NotAfter, Token)
    };

    struct _UcsMigrationResponse {
        std::vector<std::string> gcsConsentsToOverride;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(_UcsMigrationResponse, gcsConsentsToOverride)
    };

    _TitleToken           TitleToken;
    _UserToken            UserToken;          // user token
    _UserToken            AuthorizationToken; // authz token with xui
    std::string           WebPage;
    _UcsMigrationResponse UcsMigrationResponse;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(SisuToken, TitleToken, UserToken, AuthorizationToken, WebPage, UcsMigrationResponse)

    bool isExpired();
};

struct MSLOGIN_API XstsToken {
    struct DisplayClaimsType {
        struct Xui {
            std::string gtg;
            std::string xid;
            std::string uhs;
            std::string agg;
            std::string usr;
            std::string prv;
            std::string ugc;
            NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Xui, gtg, xid, uhs, agg, usr, prv, ugc)
        };
        std::vector<Xui> xui;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(DisplayClaimsType, xui)
    };

    std::string       IssueInstant;
    std::string       NotAfter;
    std::string       Token;
    DisplayClaimsType DisplayClaims;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(XstsToken, IssueInstant, NotAfter, Token, DisplayClaims)
    bool isExpired() const;
};

// ===== 游戏流令牌 (来自 xhome/auth/authenticate) =====

struct MSLOGIN_API GSToken {
    struct Region {
        std::string                name;
        std::string                baseUri;
        std::optional<std::string> networkTestHostname; // 可能为空
        bool                       isDefault;
        std::optional<std::string> systemUpdateGroups; // 可能为空
        int                        fallbackPriority;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Region, name, baseUri, networkTestHostname, isDefault, systemUpdateGroups, fallbackPriority)
    };

    struct Environment {
        std::string                Name;
        std::optional<std::string> AuthBaseUri; // 可能为空
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Environment, Name, AuthBaseUri)
    };

    struct ClientCloudSettings {
        std::vector<Environment> Environments;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(ClientCloudSettings, Environments)
    };

    struct OfferingSettings {
        bool                       allowRegionSelection;
        std::vector<Region>        regions;
        std::optional<std::string> selectableServerTypes; // 可能为空
        ClientCloudSettings        clientCloudSettings;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(OfferingSettings, allowRegionSelection, regions, selectableServerTypes, clientCloudSettings)
    };

    OfferingSettings offeringSettings;
    std::string      market;
    std::string      gsToken;
    std::string      tokenType;
    int64_t          durationInSeconds;
    uint64_t         expires_at = 0; // Unix 时间戳

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(GSToken, offeringSettings, market, gsToken, tokenType, durationInSeconds, expires_at)

    // 计算并更新到期时间（调用者需手动调用）
    void updateExpiry();
    bool isExpired() const;
};
