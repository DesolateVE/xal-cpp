#include <iostream>
#include <msal/msal.hpp>
#include <utils/logger.hpp>

int main() {

    Logger::getInstance().setLogCallback([](LogLevel level, const std::string& message) {
        switch (level) {
        case LogLevel::Debug: std::cout << "[DEBUG] " << message << std::endl; break;
        case LogLevel::Info: std::cout << "[INFO] " << message << std::endl; break;
        case LogLevel::Warning: std::cout << "[WARNING] " << message << std::endl; break;
        case LogLevel::Error: std::cerr << "[ERROR] " << message << std::endl; break;
        case LogLevel::Critical: std::cerr << "[CRITICAL] " << message << std::endl; break;
        }
    });

    MSAL msal("tokens.json", "tounaydrllmh@outlook.com", "NQzymTFh");

    return 0;
}