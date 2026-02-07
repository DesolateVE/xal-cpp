#include "ssl_utils.hpp"

#include <cctype>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

namespace ssl_utils {
// Base64
std::string Base64::encode(const std::vector<uint8_t>& data) {
    BIO* b64  = BIO_new(BIO_f_base64());
    BIO* bmem = BIO_new(BIO_s_mem());
    b64       = BIO_push(b64, bmem);

    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, data.data(), static_cast<int>(data.size()));
    BIO_flush(b64);

    BUF_MEM* bptr;
    BIO_get_mem_ptr(b64, &bptr);

    std::string result(bptr->data, bptr->length);
    BIO_free_all(b64);

    return result;
}

std::string Base64::encode_url_safe(const std::vector<uint8_t>& data) {
    std::string result = encode(data);
    result.erase(std::remove(result.begin(), result.end(), '='), result.end());
    std::replace(result.begin(), result.end(), '+', '-');
    std::replace(result.begin(), result.end(), '/', '_');
    return result;
}

std::vector<uint8_t> Base64::decode_url_safe(const std::string& input) {
    std::string base64 = input;
    std::replace(base64.begin(), base64.end(), '-', '+');
    std::replace(base64.begin(), base64.end(), '_', '/');
    while (base64.length() % 4 != 0) {
        base64 += '=';
    }
    BIO* b64  = BIO_new(BIO_f_base64());
    BIO* bmem = BIO_new_mem_buf(base64.c_str(), static_cast<int>(base64.length()));
    b64       = BIO_push(b64, bmem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    std::vector<uint8_t> result(base64.length());
    int                  decoded = BIO_read(b64, result.data(), static_cast<int>(base64.length()));
    BIO_free_all(b64);
    result.resize(decoded);
    return result;
}

uint64_t Time::get_windows_timestamp() {
    auto dur            = std::chrono::system_clock::now().time_since_epoch();
    auto unix_timestamp = std::chrono::duration_cast<std::chrono::seconds>(dur).count();
    return (unix_timestamp + 11644473600ULL) * 10000000ULL;
}

std::optional<std::chrono::system_clock::time_point> Time::parse_iso8601_utc(const std::string& s) {
    // 支持尾部 Z，忽略毫秒部分
    if (s.size() < 19) {
        return std::nullopt;
    }
    std::string core = s;
    if (!core.empty() && (core.back() == 'Z' || core.back() == 'z')) {
        core.pop_back();
    }
    auto t_pos = core.find('T');
    if (t_pos == std::string::npos) {
        return std::nullopt;
    }
    auto date = core.substr(0, t_pos);
    auto time = core.substr(t_pos + 1);

    int  year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    char dash1 = 0, dash2 = 0, colon1 = 0, colon2 = 0;

    std::istringstream ds(date);
    ds >> year >> dash1 >> month >> dash2 >> day;
    if (ds.fail() || dash1 != '-' || dash2 != '-') {
        return std::nullopt;
    }

    std::string        seconds_part;
    std::istringstream ts(time);
    ts >> hour >> colon1 >> minute >> colon2 >> seconds_part;
    if (ts.fail() || colon1 != ':' || colon2 != ':') {
        return std::nullopt;
    }
    auto dot_pos = seconds_part.find('.');
    if (dot_pos != std::string::npos) {
        seconds_part = seconds_part.substr(0, dot_pos);
    }
    try {
        second = std::stoi(seconds_part);
    } catch (...) { return std::nullopt; }

    std::tm tm{};
    tm.tm_year  = year - 1900;
    tm.tm_mon   = month - 1;
    tm.tm_mday  = day;
    tm.tm_hour  = hour;
    tm.tm_min   = minute;
    tm.tm_sec   = second;
    tm.tm_isdst = -1;

    std::time_t tt = _mkgmtime(&tm);
    if (tt < 0) {
        return std::nullopt;
    }
    return std::chrono::system_clock::from_time_t(tt);
}

// Uuid
std::string Uuid::generate_v4() {
    // 使用线程局部变量保证线程安全且每个线程只初始化一次
    static std::random_device              rd;
    static std::mt19937                    gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> variant_dis(8, 11);

    std::stringstream ss;
    ss << std::hex;
    ss << "{";

    // xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    for (int i = 0; i < 8; ++i)
        ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 4; ++i)
        ss << dis(gen);
    ss << "-";

    // 版本 4 (0100xxxx)
    ss << "4";
    for (int i = 0; i < 3; ++i)
        ss << dis(gen);
    ss << "-";

    // 变体 (10xxxxxx)
    ss << variant_dis(gen);
    for (int i = 0; i < 3; ++i)
        ss << dis(gen);
    ss << "-";

    for (int i = 0; i < 12; ++i)
        ss << dis(gen);
    ss << "}";
    return ss.str();
}

// Signature
std::vector<uint8_t> Signature::der_to_ieee_p1363(const std::vector<uint8_t>& der_signature) {
    std::vector<uint8_t> signature;
    const unsigned char* p         = der_signature.data();
    ECDSA_SIG*           ecdsa_sig = d2i_ECDSA_SIG(nullptr, &p, static_cast<long>(der_signature.size()));
    if (ecdsa_sig) {
        const BIGNUM* r = ECDSA_SIG_get0_r(ecdsa_sig);
        const BIGNUM* s = ECDSA_SIG_get0_s(ecdsa_sig);
        if (r && s) {
            std::vector<uint8_t> r_bytes(32, 0);
            std::vector<uint8_t> s_bytes(32, 0);
            BN_bn2binpad(r, r_bytes.data(), 32);
            BN_bn2binpad(s, s_bytes.data(), 32);
            signature.resize(64);
            memcpy(signature.data(), r_bytes.data(), 32);
            memcpy(signature.data() + 32, s_bytes.data(), 32);
        }
        ECDSA_SIG_free(ecdsa_sig);
    }
    if (signature.empty()) {
        throw std::runtime_error("转换 DER 签名到 IEEE P1363 格式失败");
    }
    return signature;
}

std::vector<uint8_t> Signature::easy_sign(std::vector<uint8_t>& data, EVP_PKEY* pkey) {
    EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
    EVP_DigestSignInit(md_ctx, nullptr, EVP_sha256(), nullptr, pkey);
    EVP_DigestSignUpdate(md_ctx, data.data(), data.size());
    size_t sig_len = 0;
    EVP_DigestSignFinal(md_ctx, nullptr, &sig_len);
    std::vector<uint8_t> der_signature(sig_len);
    EVP_DigestSignFinal(md_ctx, der_signature.data(), &sig_len);
    EVP_MD_CTX_free(md_ctx);

    return der_to_ieee_p1363(der_signature);
}

// URL encode
std::string url_encode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;
    for (char c : value) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            continue;
        }
        escaped << std::uppercase;
        escaped << '%' << std::setw(2) << int(static_cast<unsigned char>(c));
        escaped << std::nouppercase;
    }
    return escaped.str();
}

std::string url_encode(const nlohmann::json& value) {
    std::string body;
    for (auto it = value.begin(); it != value.end(); ++it) {
        if (it != value.begin()) {
            body += "&";
        }
        body += ssl_utils::url_encode(it.key()) + "=" + ssl_utils::url_encode(it.value().get<std::string>());
    }
    return body;
}

void WriteBuffer::write_uint8(uint8_t value) { buffer_.push_back(value); }

void WriteBuffer::write_uint32(uint32_t value) {
    buffer_.push_back(static_cast<uint8_t>(value & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

void WriteBuffer::write_uint32_be(uint32_t value) {
    buffer_.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>(value & 0xFF));
}

void WriteBuffer::write_uint64(uint64_t value) {
    buffer_.push_back(static_cast<uint8_t>(value & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 32) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 40) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 48) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 56) & 0xFF));
}

void WriteBuffer::write_uint64_be(uint64_t value) {
    buffer_.push_back(static_cast<uint8_t>((value >> 56) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 48) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 40) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 32) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>(value & 0xFF));
}

void WriteBuffer::write_string(const std::string& value) { buffer_.insert(buffer_.end(), value.begin(), value.end()); }

void WriteBuffer::write_bytes(const std::vector<uint8_t>& data) { buffer_.insert(buffer_.end(), data.begin(), data.end()); }

std::vector<uint8_t>& WriteBuffer::data() { return buffer_; }
} // namespace ssl_utils
