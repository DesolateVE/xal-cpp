#pragma once
#include <string>

namespace cpr {
class Session;
}

class MSALLogin {
    struct Credential {
        std::string ppft;
        std::string urlPost;
    };

public:
    MSALLogin();
    ~MSALLogin();

    void MSALEasyLogin(const std::string& url, const std::string& code, const std::string& email, const std::string& password);

private:
    auto GetCodeAuth(const std::string& url) -> Credential;
    auto PostCodeAuth(const Credential& credential, const std::string& code) -> Credential;
    auto PostAccountAuth(const Credential& credential, const std::string& email, const std::string& password) -> Credential;
    auto PostLoginFinish(const Credential& credential) -> void;

private:
    cpr::Session* session;
};