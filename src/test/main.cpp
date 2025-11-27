#include <iostream>

#include "xal.hpp"

int main(int argc, char** argv) {
    XAL xal(".xboxtoken.json");

    if (xal.isAuthenticating() == false) {
        auto url = xal.getRedirectUri();
        std::cout << "Please open the following URL in your browser:\n" << url << std::endl;
        std::cout << "After logging in, please enter the redirected URL: ";
        std::string redirect_url;
        std::cin >> redirect_url;
        xal.authenticateUser(redirect_url);
    }

    auto gs_token  = xal.getGSToken();
    auto web_token = xal.getWebToken();
    return 0;
}
