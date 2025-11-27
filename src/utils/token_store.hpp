#pragma once
#include <chrono>
#include <ctime>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

// 补充 std::optional 的 ADL 序列化以兼容当前环境
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

// ===== UserToken (MSA OAuth token) =====
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
struct SisuToken {
    struct TitleDisplayClaims {
        struct Xti {
            std::string tid;
            NLOHMANN_DEFINE_TYPE_INTRUSIVE(Xti, tid)
        };
        Xti xti;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(TitleDisplayClaims, xti)
    };

    struct _TitleToken {
        TitleDisplayClaims DisplayClaims;
        std::string        IssueInstant;
        std::string        NotAfter;
        std::string        Token;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(_TitleToken, DisplayClaims, IssueInstant, NotAfter, Token)
    };

    struct UserDisplayClaims {
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
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(UserDisplayClaims, xui)
    };

    struct _UserToken {
        UserDisplayClaims DisplayClaims;
        std::string       IssueInstant;
        std::string       NotAfter;
        std::string       Token;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(_UserToken, DisplayClaims, IssueInstant, NotAfter, Token)
    };

    struct _UcsMigrationResponse {
        std::vector<std::string> gcsConsentsToOverride;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(_UcsMigrationResponse, gcsConsentsToOverride)
    };

    std::string           DeviceToken;
    _TitleToken           TitleToken;
    _UserToken            UserToken;           // user token
    _UserToken            AuthorizationToken;  // authz token with xui
    std::string           WebPage;
    std::string           Sandbox;
    bool                  UseModernGamertag;
    _UcsMigrationResponse UcsMigrationResponse;
    std::string           Flow;
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

struct MsalToken {
    std::string lpt;
    std::string refresh_token;
    std::string user_id;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(MsalToken, lpt, refresh_token, user_id)
};

// ===== GSToken (Game Streaming Token from xhome/auth/authenticate) =====

struct GSToken {
    struct Region {
        std::string                name;
        std::string                baseUri;
        std::optional<std::string> networkTestHostname;  // 可能为 null
        bool                       isDefault;
        std::optional<std::string> systemUpdateGroups;  // 可能为 null
        int                        fallbackPriority;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(
            Region, name, baseUri, networkTestHostname, isDefault, systemUpdateGroups, fallbackPriority)
    };

    struct Environment {
        std::string                Name;
        std::optional<std::string> AuthBaseUri;  // 可能为 null
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Environment, Name, AuthBaseUri)
    };

    struct ClientCloudSettings {
        std::vector<Environment> Environments;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(ClientCloudSettings, Environments)
    };

    struct OfferingSettings {
        bool                       allowRegionSelection;
        std::vector<Region>        regions;
        std::optional<std::string> selectableServerTypes;  // 可能为 null
        ClientCloudSettings        clientCloudSettings;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(
            OfferingSettings, allowRegionSelection, regions, selectableServerTypes, clientCloudSettings)
    };

    OfferingSettings offeringSettings;
    std::string      market;
    std::string      gsToken;
    std::string      tokenType;
    int64_t          durationInSeconds;
    uint64_t         expires_at = 0;  // Unix 时间戳

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
        GSToken, offeringSettings, market, gsToken, tokenType, durationInSeconds, expires_at)

    // 计算并更新到期时间（调用者需手动调用）
    void updateExpiry();
    bool isExpired() const;
};
