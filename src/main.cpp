#include <iostream>

#include "xal/xal.hpp"

int main(int argc, char** argv) {
    XAL         xal("device_token.json");
    std::string redirect_url;

    auto url = xal.getRedirectUri();
    std::cout << "Please open the following URL in your browser:\n" << url << std::endl;
    std::cout << "After logging in, please enter the redirected URL: ";
    std::cin >> redirect_url;
    xal.authenticateUser(redirect_url);
    xal.getHomeStreamingToken();
    return 0;
}
