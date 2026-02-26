#pragma once
#include <string>

namespace LoginManager {

    void Login(const std::string& user,
               const std::string& pass,
               const std::string& id);

    void Logout();

    // 🔹 NUEVAS FUNCIONES
    bool IsLogged();
    bool IsRoot();
    std::string GetSessionId();
    std::string GetUser();

}