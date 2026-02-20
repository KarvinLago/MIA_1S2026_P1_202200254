#pragma once
#include <fstream>
#include <string>

namespace FileUtils {

bool CreateDiskFile(const std::string& path);
std::fstream OpenFile(const std::string& path);

}