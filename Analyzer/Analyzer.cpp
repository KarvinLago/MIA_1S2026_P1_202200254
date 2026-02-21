// #include "Analyzer.h"
// #include "../Disk/DiskManager.h"
// #include "../Mount/MountManager.h"
// #include <iostream>
// #include <sstream>
// #include <map>
// #include <regex>
// #include <algorithm>

// namespace Analyzer {

// void Analyze()
// {
//     std::string input;
//     std::getline(std::cin, input);

//     if(input.empty()) return;

//     std::istringstream iss(input);
//     std::string command;
//     iss >> command;

//     std::transform(command.begin(), command.end(), command.begin(), ::tolower);

//     std::regex re(R"(-(\w+)=("[^"]+"|\S+))");
//     std::sregex_iterator it(input.begin(), input.end(), re);
//     std::sregex_iterator end;

//     std::map<std::string,std::string> params;

//     for(; it != end; ++it){
//         std::string key = it->str(1);
//         std::string value = it->str(2);

//         if(value.front() == '"' && value.back() == '"'){
//             value = value.substr(1, value.size()-2);
//         }

//         std::transform(key.begin(), key.end(), key.begin(), ::tolower);
//         params[key] = value;
//     }

//     // ================= MKDISK =================
//     if(command == "mkdisk"){
//         DiskManager::Mkdisk(
//             std::stoi(params["size"]),
//             params["fit"][0],
//             params["unit"][0],
//             params["path"]
//         );
//     }

//     // ================= FDISK =================
//     else if(command == "fdisk"){
//         DiskManager::Fdisk(
//             std::stoi(params["size"]),
//             params["path"],
//             params["name"],
//             params.count("type") ? params["type"][0] : 'p',
//             params.count("fit")  ? params["fit"][0]  : 'f',
//             params.count("unit") ? params["unit"][0] : 'k'
//         );
//     }

//     else{
//         std::cout << "Comando no reconocido\n";
//     }
// }

// }


#include "Analyzer.h"
#include "../Disk/DiskManager.h"
#include "../Mount/MountManager.h"
#include <iostream>
#include <sstream>
#include <map>
#include <regex>
#include <algorithm>

namespace Analyzer {

void Analyze()
{
    std::string input;
    std::getline(std::cin, input);

    if(input.empty()) return;

    std::istringstream iss(input);
    std::string command;
    iss >> command;

    std::transform(command.begin(), command.end(), command.begin(), ::tolower);

    std::regex re(R"(-(\w+)=("[^"]+"|\S+))");
    std::sregex_iterator it(input.begin(), input.end(), re);
    std::sregex_iterator end;

    std::map<std::string,std::string> params;

    for(; it != end; ++it){
        std::string key = it->str(1);
        std::string value = it->str(2);

        if(value.front() == '"' && value.back() == '"'){
            value = value.substr(1, value.size()-2);
        }

        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        params[key] = value;
    }

    // ================= MKDISK =================
    if(command == "mkdisk"){
        DiskManager::Mkdisk(
            std::stoi(params["size"]),
            params["fit"][0],
            params["unit"][0],
            params["path"]
        );
    }

    // ================= FDISK =================
    else if(command == "fdisk"){
        DiskManager::Fdisk(
            std::stoi(params["size"]),
            params["path"],
            params["name"],
            params.count("type") ? params["type"][0] : 'p',
            params.count("fit")  ? params["fit"][0]  : 'f',
            params.count("unit") ? params["unit"][0] : 'k'
        );
    }

    // ================= MOUNT =================
    else if(command == "mount"){
        if(!params.count("path") || !params.count("name")){
            std::cout << "Error: mount requiere -path y -name\n";
            return;
        }

        MountManager::Mount(
            params["path"],
            params["name"]
        );
    }

    // ================= MOUNTED =================
    else if(command == "mounted"){
        MountManager::ShowMounted();
    }

    else{
        std::cout << "Comando no reconocido\n";
    }
}

}