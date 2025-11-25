#pragma once

#include <openssl/evp.h>

#include <memory>
#include <string>

#include "ssl_utils.hpp"

class JwtKey {
public:
    JwtKey();
    ~JwtKey();

    // 生成新的 P-256 密钥对
    void Generate();

    // 序列化为 JSON 字符串（包含公钥 x,y 和私钥 d）
    std::string Serialize() const;

    // 从 JSON 字符串反序列化
    bool Deserialize(const std::string& json);

    // 获取 Base64URL 编码的公钥坐标
    const std::string& GetX() const { return x_; }
    const std::string& GetY() const { return y_; }

    // 获取 Base64URL 编码的私钥
    const std::string& GetD() const { return d_; }

    // 获取 OpenSSL EVP_PKEY 指针（用于签名等操作）
    EVP_PKEY* GetEVP_PKEY() const { return pkey_.get(); }

    // 检查密钥是否有效
    bool IsValid() const { return pkey_ != nullptr; }

private:
    std::string x_;
    std::string y_;
    std::string d_;

    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> pkey_{nullptr, &EVP_PKEY_free};
};