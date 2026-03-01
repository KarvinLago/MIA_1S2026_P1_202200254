#include "Analyzer.h"
#include "../Disk/DiskManager.h"
#include "../Mount/MountManager.h"
#include "../FileSystem/MkfsManager.h"
#include "../Auth/LoginManager.h"
#include "../Users/GroupManager.h"
#include "../Users/UserManager.h"
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
        if(!params.count("size") || !params.count("fit") ||
           !params.count("unit") || !params.count("path")){
            std::cout << "Error: mkdisk requiere -size -fit -unit -path\n";
            return;
        }

        DiskManager::Mkdisk(
            std::stoi(params["size"]),
            params["fit"][0],
            params["unit"][0],
            params["path"]
        );
    }

    // ================= FDISK =================
    else if(command == "fdisk"){
        if(!params.count("size") || !params.count("path") ||
           !params.count("name")){
            std::cout << "Error: fdisk requiere -size -path -name\n";
            return;
        }

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

    // ================= MKFS =================
    else if(command == "mkfs"){
        if(!params.count("id")){
            std::cout << "Error: mkfs requiere -id\n";
            return;
        }

        MkfsManager::Mkfs(params["id"]);
    }

    // ================= LOGIN =================
    else if(command == "login"){
        if(!params.count("user") || !params.count("pass") ||
           !params.count("id")){
            std::cout << "Error: login requiere -user -pass -id\n";
            return;
        }

        LoginManager::Login(
            params["user"],
            params["pass"],
            params["id"]
        );
    }

    // ================= LOGOUT =================
    else if(command == "logout"){
        LoginManager::Logout();
    }

    // ================= MKGRP =================
    else if(command == "mkgrp"){
        if(!params.count("name")){
            std::cout << "Error: mkgrp requiere -name\n";
            return;
        }

        GroupManager::Mkgrp(params["name"]);
    }

    // ================= RMGRP =================
    else if(command == "rmgrp"){
        if(!params.count("name")){
            std::cout << "Error: rmgrp requiere -name\n";
            return;
        }

        GroupManager::Rmgrp(params["name"]);
    }

    // ================= MKUSR =================
    else if(command == "mkusr"){
        if(!params.count("user") ||
           !params.count("pass") ||
           !params.count("grp")){
            std::cout << "Error: mkusr requiere -user -pass -grp\n";
            return;
        }

        UserManager::Mkusr(
            params["user"],
            params["pass"],
            params["grp"]
        );
    }

    // ================= RMUSR =================
    else if(command == "rmusr"){
        if(!params.count("user")){
            std::cout << "Error: rmusr requiere -user\n";
            return;
        }

        UserManager::Rmusr(params["user"]);
    }




    // ================= COMANDO NO RECONOCIDO =================
    else{
        std::cout << "Comando no reconocido\n";
    }
}

}