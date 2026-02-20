#pragma once
#include <string>

namespace DiskManager {
    void Mkdisk(int size, char fit, char unit, const std::string& path);
}