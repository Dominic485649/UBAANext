#pragma once

#include <string>

namespace UBAANext {
namespace Model {

struct WifiCredentials {
    std::string username;
    std::string password;
};

struct WifiResult {
    std::string action;
    std::string username;
    std::string ip;
    std::string ac_id;
    std::string status;
    std::string message;
};

} // namespace Model
} // namespace UBAANext
