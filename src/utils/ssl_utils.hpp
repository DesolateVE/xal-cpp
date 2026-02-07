#pragma once

#include <chrono>
#include <nlohmann/json.hpp>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>
#include <openssl/pem.h>
#include <openssl/sha.h>
#include <optional>
#include <string>
#include <vector>


namespace ssl_utils {
namespace Base64 {
std::string          encode(const std::vector<uint8_t>& data);
std::string          encode_url_safe(const std::vector<uint8_t>& data);
std::vector<uint8_t> decode_url_safe(const std::string& input);
} // namespace Base64

namespace Time {
uint64_t get_windows_timestamp();
// 解析 ISO8601 UTC 时间字符串，例如 "2025-11-26T08:32:10.5118384Z"
// 成功返回时间点，失败返回空值
std::optional<std::chrono::system_clock::time_point> parse_iso8601_utc(const std::string& s);
// 获取网络 NTP 时间（从 NTP 服务器获取），网络不可用时返回本地时间
std::chrono::system_clock::time_point get_network_time();
} // namespace Time

namespace Uuid {
std::string generate_v4();
} // namespace Uuid

namespace Signature {
std::vector<uint8_t> der_to_ieee_p1363(const std::vector<uint8_t>& der_signature);
std::vector<uint8_t> easy_sign(std::vector<uint8_t>& data, EVP_PKEY* pkey);
} // namespace Signature

std::string url_encode(const std::string& value);
std::string url_encode(const nlohmann::json& value);

class WriteBuffer {
public:
    void write_uint8(uint8_t value);
    void write_uint32(uint32_t value);
    void write_uint32_be(uint32_t value);
    void write_uint64(uint64_t value);
    void write_uint64_be(uint64_t value);
    void write_string(const std::string& value);
    void write_bytes(const std::vector<uint8_t>& data);

    std::vector<uint8_t>& data();

private:
    std::vector<uint8_t> buffer_;
};
} // namespace ssl_utils