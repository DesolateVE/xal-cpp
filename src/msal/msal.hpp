#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>

#include "mslogin_export.hpp"
#include "utils/token_store.hpp"

class MSLOGIN_API MSAL {

    struct Credential {
        std::string ppft;
        std::string urlPost;
    };

public:
    MSAL(const std::filesystem::path& path, const std::string& email, const std::string& password);
    ~MSAL();

    MSAL_OAuth2Token getOAuth2Token();
    XstsToken        getXstsToken();
    XstsToken        getXBLToken();
    XstsToken        getGssvToken();
    GSToken          getStreamToken();

private:
    void fullLogin();

    void doDeviceCodeAuth(MSAL_IDeviceCodeAuth& out_device_code);
    void doPollForDeviceCodeAuth(const std::string& device_code, MSAL_OAuth2Token& out_oauth2_token);
    void doXstsAuthentication(const std::string& oauth2_token, XstsToken& out_xsts_token);
    void getGssvToken(const std::string& xsts_token, XstsToken& out_gssv_token);
    void getXBLToken(const std::string& xsts_token, XstsToken& out_xbl_token);
    void getStreamHomeToken(const std::string& gssv_token, GSToken& out_stream_token);
    void getXALToken(const std::string& refresh_token, XAL_UserToken& out_token);

    void doXstsAuthorization(const std::string& xsts_token, const std::string& relying_party, XstsToken& out_token);
    void getStreamToken(const std::string& gssv_token, const std::string& offering, GSToken& out_stream_token);
    void refreshOAtuh2Token(const std::string& refresh_token, MSAL_OAuth2Token& out_token);

    void loadTokensFromFile();
    void saveTokensToFile();
    void loginWithCredentials();

private:
    std::filesystem::path _tokenFilePath;
    MSAL_OAuth2Token      _oauth2Token;
    XstsToken             _xstsToken;
    XstsToken             _xblToken;
    XstsToken             _gssvToken;
    GSToken               _streamToken;
    std::string           _email;
    std::string           _password;
};