#include "token_store.hpp"

#include <chrono>

#include "ssl_utils.hpp"

// ---------------- UserToken implementations ----------------
void XAL_UserToken::updateExpiry() {
    auto now            = std::chrono::system_clock::now();
    auto unix_timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    expires_at          = unix_timestamp + expires_in;
}

bool XAL_UserToken::isExpired() const {
    auto now            = std::chrono::system_clock::now();
    auto unix_timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    return unix_timestamp >= expires_at;
}

// ---------------- SisuToken implementation ----------------
bool SisuToken::isExpired() {
    auto               now      = std::chrono::system_clock::now();
    const std::string* fields[] = {&TitleToken.NotAfter, &UserToken.NotAfter, &AuthorizationToken.NotAfter};
    for (auto* f : fields) {
        if (!f->empty()) {
            auto tp = ssl_utils::Time::parse_iso8601_utc(*f);
            if (!tp || now >= *tp)
                return true; // 解析失败或已过期则返回真
        }
    }
    return false;
}

// ---------------- GSToken implementation ----------------
void GSToken::updateExpiry() {
    auto now     = std::chrono::system_clock::now();
    auto now_sec = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    expires_at   = now_sec + durationInSeconds;
}

bool GSToken::isExpired() const {
    auto now     = std::chrono::system_clock::now();
    auto now_sec = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    return now_sec >= expires_at;
}

// ==== XstsToken implementations ====
bool XstsToken::isExpired() const {
    if (NotAfter.empty()) {
        return true;
    }
    auto not_after_tp = ssl_utils::Time::parse_iso8601_utc(NotAfter);
    if (!not_after_tp)
        return true; // 解析失败视为过期
    auto now = std::chrono::system_clock::now();
    return now >= *not_after_tp;
}

void MSAL_OAuth2Token::updateExpiry() {
    auto now            = std::chrono::system_clock::now();
    auto unix_timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    expires_at          = unix_timestamp + expires_in;
}

bool MSAL_OAuth2Token::isExpired() const {
    auto now            = std::chrono::system_clock::now();
    auto unix_timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    return unix_timestamp >= expires_at;
}
