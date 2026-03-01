#pragma once
#include <string>

namespace UserManager {

void Mkusr(const std::string& user,
           const std::string& pass,
           const std::string& grp);

void Rmusr(const std::string& user);

}