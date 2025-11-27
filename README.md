# xal-cpp

This project is a C++ implementation inspired by and functionally aligned with the Node.js project "xal-node" at: https://github.com/unknownskl/xal-node. Design, flow, and token models follow that reference; this repository provides a native C++ implementation.

- Build system: xmake
- Language: C++20
- Key libs: OpenSSL, nlohmann_json, cpp-httplib, Boost (scope_exit)

## Overview

```cpp
class XAL
{
public:
    XAL(std::filesystem::path token_file);
    ~XAL();

    std::string getLoginUri();
    void        authenticateUser(std::string redirectUri);
    bool        isAuthenticating() const { return mIsAuthenticating; }

    UserToken getUserToken();
    SisuToken getSisuToken();
    MsalToken getMsalToken();
    XstsToken getWebToken();
    GSToken   getGSToken();
};
```

## Build & Run

Prerequisites: xmake, a C++20 toolchain (MSVC/Clang), OpenSSL, etc.

- Configure and build:
```
xmake build
```
- Run tests/example:
```
xmake run MSLoginTest
```

## Attribution

- Reference implementation: https://github.com/unknownskl/xal-node
- This repository is a C++ implementation following the same authentication flow and token models.
