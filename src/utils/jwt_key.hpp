#pragma once

#include <openssl/evp.h>

#include <memory>
#include <nlohmann/json.hpp>
#include <string>

class JwtKey {
public:
    JwtKey();
    ~JwtKey();

    // 生成新的 P-256 密钥对
    void Generate();

    // 从成员变量反序列化为 OpenSSL 密钥对象
    void Deserialize();

    // 获取 Base64URL 编码的公钥 X、Y 坐标
    const std::string& GetX() const { return x_; }
    const std::string& GetY() const { return y_; }

    // 获取 Base64URL 编码的私钥 D 值
    const std::string& GetD() const { return d_; }

    // 获取 OpenSSL 密钥对象指针（用于签名等操作）
    EVP_PKEY* GetEVP_PKEY() const { return pkey_; }

    // 检查密钥是否有效
    bool IsValid() const { return pkey_ != nullptr; }

public:
    std::string x_;
    std::string y_;
    std::string d_;
    EVP_PKEY*   pkey_ = nullptr;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(JwtKey, x_, y_, d_)
};