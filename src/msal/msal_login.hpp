#pragma once
#include <string>

namespace cpr {
class Session;
}

namespace Regular {
struct FormData;
}

class MSALLogin {
public:
    MSALLogin();
    ~MSALLogin();
    void MSALEasyLogin(const std::string& url, const std::string& code, const std::string& email, const std::string& password);

private:
    auto GetCodeAuth(const std::string& url, std::string& sFT, std::string& urlPost) -> void;

    auto PostCodeAuth(
        const std::string& url,
        const std::string& sFT,
        const std::string& code,
        std::string&       urlPost,
        std::string&       sRandomBlob,
        std::string&       newSFTTag
    ) -> void;

    auto PostAccountAuth(
        const std::string& url,
        const std::string& sFT,
        const std::string& sRandomBlob,
        const std::string& email,
        const std::string& password
    ) -> void;

    /// @brief 提交标准的 type=28 最终认证
    auto SubmitFinalAuth(const std::string& urlPost, const std::string& sftTag) -> void;

    /// @brief 处理 account.live.com/proofs/remind 分支（确认账号信息）
    auto HandleProofsRemind(const Regular::FormData& formData, const std::string& email, const std::string& password) -> void;

    /// @brief 处理 account.live.com/interrupt/passkey 分支（跳过通行密钥申请）
    auto HandlePasskeyInterrupt(const Regular::FormData& formData) -> void;

    /// @brief 处理 title=="继续" 的页面，根据 form action 分发到各子分支
    auto HandleContinuePage(const std::string& html, const std::string& email, const std::string& password) -> void;

private:
    cpr::Session* session;
};