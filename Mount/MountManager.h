#pragma once
#include <string>
#include <vector>

struct MountedPartition {
    std::string path;
    std::string name;
    std::string id;
};

namespace MountManager {

    void Mount(const std::string& path,
               const std::string& name);

    void ShowMounted();
}