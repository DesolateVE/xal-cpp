#include "jwt_key.hpp"

#include <boost/scope_exit.hpp>

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/obj_mac.h>

#include <nlohmann/json.hpp>
#include <stdexcept>
#include <vector>

#include "ssl_utils.hpp"

JwtKey::JwtKey() {}

JwtKey::~JwtKey() {
    if (pkey_) {
        EVP_PKEY_free(pkey_);
        pkey_ = nullptr;
    }
}

void JwtKey::Generate() {
    EVP_PKEY_CTX* pctx   = nullptr;
    EVP_PKEY*     pkey   = nullptr;
    EC_KEY*       ec_key = nullptr;
    BIGNUM*       x      = nullptr;
    BIGNUM*       y      = nullptr;

    BOOST_SCOPE_EXIT(&pctx, &pkey, &ec_key, &x, &y) {
        if (pctx) EVP_PKEY_CTX_free(pctx);
        if (pkey) EVP_PKEY_free(pkey);
        if (ec_key) EC_KEY_free(ec_key);
        if (x) BN_free(x);
        if (y) BN_free(y);
    }
    BOOST_SCOPE_EXIT_END

    // 1. 创建密钥生成上下文
    pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
    if (pctx == nullptr) { throw std::runtime_error("创建 EVP_PKEY_CTX 失败"); }

    // 2. 初始化密钥生成
    if (EVP_PKEY_keygen_init(pctx) <= 0) { throw std::runtime_error("初始化密钥生成失败"); }

    // 3. 设置 P-256 曲线
    if (EVP_PKEY_CTX_set_ec_paramgen_curve_nid(pctx, NID_X9_62_prime256v1) <= 0) {
        throw std::runtime_error("设置 P-256 曲线失败");
    }

    // 4. 生成密钥对
    if (EVP_PKEY_keygen(pctx, &pkey) <= 0) { throw std::runtime_error("生成密钥对失败"); }

    // 5. 提取底层 EC_KEY
    ec_key = EVP_PKEY_get1_EC_KEY(pkey);
    if (ec_key == nullptr) { throw std::runtime_error("从 EVP_PKEY 获取 EC_KEY 失败"); }

    // 6. 获取公钥点和群参数
    const EC_POINT* pub_key = EC_KEY_get0_public_key(ec_key);
    const EC_GROUP* group   = EC_KEY_get0_group(ec_key);
    if (pub_key == nullptr || group == nullptr) { throw std::runtime_error("获取公钥点或群参数失败"); }

    // 7. 提取公钥坐标 (x, y)
    x = BN_new();
    y = BN_new();
    if (x == nullptr || y == nullptr) { throw std::runtime_error("分配 BIGNUM 失败"); }

    if (EC_POINT_get_affine_coordinates_GFp(group, pub_key, x, y, nullptr) != 1) {
        throw std::runtime_error("获取仿射坐标失败");
    }

    // 8. 转换为 32 字节固定长度并 Base64URL 编码
    std::vector<uint8_t> x_bytes(32), y_bytes(32);
    BN_bn2binpad(x, x_bytes.data(), 32);
    BN_bn2binpad(y, y_bytes.data(), 32);

    x_ = ssl_utils::Base64::encode_url_safe(x_bytes);
    y_ = ssl_utils::Base64::encode_url_safe(y_bytes);

    // 9. 提取私钥 d
    const BIGNUM* private_bn = EC_KEY_get0_private_key(ec_key);
    if (private_bn) {
        std::vector<uint8_t> d_bytes(32);
        BN_bn2binpad(private_bn, d_bytes.data(), 32);
        d_ = ssl_utils::Base64::encode_url_safe(d_bytes);
    }

    // 10. 转移所有权到成员变量
    pkey_ = pkey;
    pkey  = nullptr;  // 防止 SCOPE_EXIT 释放
}

void JwtKey::Deserialize() {
    // 解码为字节数组
    std::vector<uint8_t> x_bytes = ssl_utils::Base64::decode_url_safe(x_);
    std::vector<uint8_t> y_bytes = ssl_utils::Base64::decode_url_safe(y_);
    std::vector<uint8_t> d_bytes = ssl_utils::Base64::decode_url_safe(d_);

    if (x_bytes.size() != 32 || y_bytes.size() != 32 || d_bytes.size() != 32) {
        throw std::runtime_error("无效的密钥长度");
    }

    // 从坐标重建 EC_KEY
    EC_KEY* ec_key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
    if (!ec_key) { throw std::runtime_error("创建 EC_KEY 失败"); }

    // 标记是否成功，控制是否释放 ec_key
    bool success = false;
    BOOST_SCOPE_EXIT(&ec_key, &success) {
        if (!success && ec_key) EC_KEY_free(ec_key);
    }
    BOOST_SCOPE_EXIT_END

    const EC_GROUP* group = EC_KEY_get0_group(ec_key);
    BIGNUM*         bn_x  = BN_bin2bn(x_bytes.data(), 32, nullptr);
    BIGNUM*         bn_y  = BN_bin2bn(y_bytes.data(), 32, nullptr);
    BIGNUM*         bn_d  = BN_bin2bn(d_bytes.data(), 32, nullptr);

    if (!bn_x || !bn_y || !bn_d) {
        if (bn_x) BN_free(bn_x);
        if (bn_y) BN_free(bn_y);
        if (bn_d) BN_free(bn_d);
        throw std::runtime_error("创建 BIGNUM 失败");
    }

    BOOST_SCOPE_EXIT(&bn_x, &bn_y, &bn_d) {
        BN_free(bn_x);
        BN_free(bn_y);
        BN_free(bn_d);
    }
    BOOST_SCOPE_EXIT_END

    // 设置公钥点
    EC_POINT* pub_point = EC_POINT_new(group);
    if (!pub_point) { throw std::runtime_error("创建 EC_POINT 失败"); }

    BOOST_SCOPE_EXIT(&pub_point) {
        EC_POINT_free(pub_point);
    }
    BOOST_SCOPE_EXIT_END

    if (EC_POINT_set_affine_coordinates_GFp(group, pub_point, bn_x, bn_y, nullptr) != 1) {
        throw std::runtime_error("设置仿射坐标失败");
    }

    if (EC_KEY_set_public_key(ec_key, pub_point) != 1) { throw std::runtime_error("设置公钥失败"); }

    // 设置私钥
    if (EC_KEY_set_private_key(ec_key, bn_d) != 1) { throw std::runtime_error("设置私钥失败"); }

    // 验证密钥对
    if (EC_KEY_check_key(ec_key) != 1) { throw std::runtime_error("无效的密钥对"); }

    // 转换为 EVP_PKEY
    EVP_PKEY* pkey = EVP_PKEY_new();
    if (!pkey) { throw std::runtime_error("创建 EVP_PKEY 失败"); }

    if (EVP_PKEY_assign_EC_KEY(pkey, ec_key) != 1) {
        EVP_PKEY_free(pkey);
        throw std::runtime_error("分配 EC_KEY 给 EVP_PKEY 失败");
    }

    // 标记成功，阻止 SCOPE_EXIT 释放 ec_key（所有权已转移）
    success = true;

    // 释放旧的 pkey_ 并保存新值
    if (pkey_) EVP_PKEY_free(pkey_);
    pkey_ = pkey;
}
