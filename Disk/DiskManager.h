#pragma once
#include <string>

namespace DiskManager {

    void Mkdisk(int size, char fit, char unit, const std::string& path);

    void Fdisk(int size,
               const std::string& path,
               const std::string& name,
               char type,
               char fit,
               char unit);
}