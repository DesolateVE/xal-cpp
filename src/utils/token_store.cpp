#include "token_store.hpp"
#include "ssl_utils.hpp"
#include <sstream>
#include <optional>

bool SisuToken::isExpired() {

    auto now     = std::chrono::system_clock::now();
    auto now_sec = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    // Collect NotAfter fields to check
    const std::string* fields[] = {&TitleToken.NotAfter, &UserToken.NotAfter, &AuthorizationToken.NotAfter};
    for (auto* f: fields) {
        if (!f->empty()) {
            auto tp = ssl_utils::Time::parse_iso8601_utc(*f);
            // 统一语义：解析失败视为过期
            if (!tp) return true;
            auto tt = std::chrono::system_clock::to_time_t(*tp);
            if (static_cast<int64_t>(tt) <= now_sec) return true;  // expired
        }
    }
    return false;
}

// ==== UserToken implementations moved from header ====
void UserToken::updateExpiry() {
    auto now            = std::chrono::system_clock::now();
    auto unix_timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    expires_at          = unix_timestamp + expires_in;
}

bool UserToken::isExpired() const {
    auto now            = std::chrono::system_clock::now();
    auto unix_timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    return unix_timestamp >= expires_at;
}

// ==== XstsToken implementations moved from header ====
std::chrono::system_clock::time_point XstsToken::parse_iso8601_utc(const std::string& s) {
    auto tp = ssl_utils::Time::parse_iso8601_utc(s);
    return tp.value_or(std::chrono::system_clock::time_point{});
}

bool XstsToken::isExpired() const {
    if (NotAfter.empty()) { return true; }
    auto not_after_tp = parse_iso8601_utc(NotAfter);
    auto now          = std::chrono::system_clock::now();
    return now >= not_after_tp;
}

uint64_t XstsToken::secondsUntilExpiry() const {
    if (NotAfter.empty()) { return 0; }
    auto not_after_tp = parse_iso8601_utc(NotAfter);
    auto now          = std::chrono::system_clock::now();
    if (now >= not_after_tp) { return 0; }
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(not_after_tp - now).count());
}
