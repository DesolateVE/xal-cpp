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

    void updateExpiry() {
        auto now            = std::chrono::system_clock::now();
        auto unix_timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        expires_at          = unix_timestamp + expires_in;
    }

    bool isExpired() const {
        auto now            = std::chrono::system_clock::now();
        auto unix_timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        return unix_timestamp >= expires_at;
    }
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
    std::string Token;
};