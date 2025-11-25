#include "token_store.hpp"

bool SisuToken::isExpired() {
    // Helper: parse ISO8601 UTC like "2025-11-25T02:25:48.87587Z" (ignore fractional seconds)
    auto parse_utc = [](const std::string& s) -> std::optional<std::time_t> {
        if (s.size() < 20) return std::nullopt;  // minimal length
        // Extract "YYYY-MM-DDTHH:MM:SS"
        std::string core = s.substr(0, 19);
        if (core[4] != '-' || core[7] != '-' || core[10] != 'T' || core[13] != ':' || core[16] != ':')
            return std::nullopt;
        std::tm tm{};
        try {
            tm.tm_year = std::stoi(core.substr(0, 4)) - 1900;
            tm.tm_mon  = std::stoi(core.substr(5, 2)) - 1;
            tm.tm_mday = std::stoi(core.substr(8, 2));
            tm.tm_hour = std::stoi(core.substr(11, 2));
            tm.tm_min  = std::stoi(core.substr(14, 2));
            tm.tm_sec  = std::stoi(core.substr(17, 2));
        } catch (...) { return std::nullopt; }

        std::time_t tt = _mkgmtime(&tm);

        if (tt == static_cast<std::time_t>(-1)) return std::nullopt;
        return tt;
    };

    auto now     = std::chrono::system_clock::now();
    auto now_sec = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    // Collect NotAfter fields to check
    const std::string* fields[] = {&TitleToken.NotAfter, &UserToken.NotAfter, &AuthorizationToken.NotAfter};
    for (auto* f: fields) {
        if (!f->empty()) {
            auto t = parse_utc(*f);
            if (t && static_cast<int64_t>(*t) <= now_sec) return true;  // expired
        }
    }
    return false;
}
