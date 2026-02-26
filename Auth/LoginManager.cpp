#include "LoginManager.h"
#include "../Mount/MountManager.h"
#include "../Structs/Structs.h"
#include "../Utilities/FileUtils.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <cstring>

extern std::vector<MountedPartition> mountedList;

struct Session {
    bool active = false;
    std::string user;
    std::string id;
};

static Session currentSession;

namespace LoginManager {

void Login(const std::string& user,
           const std::string& pass,
           const std::string& id)
{
    if(currentSession.active){
        std::cout << "Error: Ya existe una sesión activa\n";
        return;
    }

    MountedPartition* mounted = nullptr;

    for(auto& m : mountedList){
        if(m.id == id){
            mounted = &m;
            break;
        }
    }

    if(mounted == nullptr){
        std::cout << "Error: ID no encontrado\n";
        return;
    }

    auto file = FileUtils::OpenFile(mounted->path);
    if(!file.is_open()){
        std::cout << "Error: No se pudo abrir el disco\n";
        return;
    }

    MBR mbr{};
    file.seekg(0);
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

    Partition* partition = nullptr;

    for(int i=0;i<4;i++){
        if(mbr.mbr_partitions[i].part_s > 0){
            if(std::string(mbr.mbr_partitions[i].part_name) == mounted->name){
                partition = &mbr.mbr_partitions[i];
                break;
            }
        }
    }

    if(partition == nullptr){
        std::cout << "Error: Partición no encontrada\n";
        file.close();
        return;
    }

    int start = partition->part_start;

    SuperBlock sb{};
    file.seekg(start);
    file.read(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));

    Inode usersInode{};
    file.seekg(sb.s_inode_start + sizeof(Inode));
    file.read(reinterpret_cast<char*>(&usersInode), sizeof(Inode));

    FileBlock usersBlock{};
    file.seekg(sb.s_block_start + sizeof(FileBlock));
    file.read(reinterpret_cast<char*>(&usersBlock), sizeof(FileBlock));

    std::string content(usersBlock.b_content);

    std::stringstream ss(content);
    std::string line;
    bool valid = false;

    while(std::getline(ss, line)){
        if(line.empty()) continue;

        std::vector<std::string> tokens;
        std::stringstream ls(line);
        std::string token;

        while(std::getline(ls, token, ',')){
            tokens.push_back(token);
        }

        if(tokens.size() == 5 && tokens[1] == "U"){
            if(tokens[3] == user && tokens[4] == pass){
                valid = true;
                break;
            }
        }
    }

    if(valid){
        currentSession.active = true;
        currentSession.user = user;
        currentSession.id = id;
        std::cout << "Login exitoso\n";
    } else {
        std::cout << "Error: Credenciales incorrectas\n";
    }

    file.close();
}

void Logout()
{
    if(!currentSession.active){
        std::cout << "Error: No hay sesión activa\n";
        return;
    }

    currentSession = Session{};
    std::cout << "Logout exitoso\n";
}

//NUEVAS FUNCIONES

bool IsLogged()
{
    return currentSession.active;
}

bool IsRoot()
{
    return currentSession.active && currentSession.user == "root";
}

std::string GetSessionId()
{
    return currentSession.id;
}

std::string GetUser()
{
    return currentSession.user;
}

}