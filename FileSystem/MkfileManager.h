#pragma once
#include <string>

namespace MkfileManager {

void Mkfile(const std::string& path,
            bool createParents,
            int size,
            const std::string& cont);

}