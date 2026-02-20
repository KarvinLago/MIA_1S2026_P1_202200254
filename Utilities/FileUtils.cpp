#include "FileUtils.h"
#include <filesystem>

namespace FileUtils {

bool CreateDiskFile(const std::string& path) {
    std::filesystem::create_directories(
        std::filesystem::path(path).parent_path()
    );

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    return file.good();
}

std::fstream OpenFile(const std::string& path) {
    return std::fstream(path, std::ios::in | std::ios::out | std::ios::binary);
}

}