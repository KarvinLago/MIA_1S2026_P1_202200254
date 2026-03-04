#include "UserManager.h"
#include "../Auth/LoginManager.h"
#include "../Mount/MountManager.h"
#include "../Structs/Structs.h"
#include "../Utilities/FileUtils.h"
#include "../FileSystem/BlockManager.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <cstring>

extern std::vector<MountedPartition> mountedList;

static bool OpenAndFind(const std::string& mountedId,
                        std::fstream& file,
                        SuperBlock& sb,
                        int& partStart)
{
    MountedPartition* mounted = nullptr;
    for(auto& m : mountedList)
        if(m.id == mountedId){ mounted = &m; break; }
    if(!mounted){ std::cout << "Error: Partición no encontrada\n"; return false; }

    file = FileUtils::OpenFile(mounted->path);
    if(!file.is_open()){ std::cout << "Error: No se pudo abrir el disco\n"; return false; }

    int partSize = -1;
    if(!FileUtils::FindPartition(file, mounted->name, partStart, partSize)){
        std::cout << "Error: Partición no encontrada en disco\n";
        file.close(); return false;
    }
    file.seekg(partStart);
    file.read(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));
    return true;
}

namespace UserManager {

void Mkusr(const std::string& user, const std::string& pass, const std::string& grp)
{
    if(!LoginManager::IsLogged()){ std::cout << "Error: Debe iniciar sesión\n"; return; }
    if(!LoginManager::IsRoot())  { std::cout << "Error: Solo root puede ejecutar mkusr\n"; return; }
    if(user.size() > 10 || pass.size() > 10 || grp.size() > 10){
        std::cout << "Error: user, pass y grp máximo 10 caracteres\n"; return;
    }

    std::fstream file;
    SuperBlock sb{};
    int partStart = -1;
    if(!OpenAndFind(LoginManager::GetSessionId(), file, sb, partStart)) return;

    Inode usersInode{};
    file.seekg(sb.s_inode_start + sizeof(Inode));
    file.read(reinterpret_cast<char*>(&usersInode), sizeof(Inode));
    std::string content = BlockManager::ReadFileContent(file, sb, usersInode);

    std::stringstream ss(content);
    std::string line;
    int maxId = 0;
    bool groupExists = false;

    while(std::getline(ss, line)){
        if(line.empty()) continue;
        std::vector<std::string> tokens;
        std::stringstream ls(line); std::string token;
        while(std::getline(ls, token, ',')) tokens.push_back(token);
        if(tokens.size() >= 3){
            if(tokens[1] == "G" && tokens[2] == grp && tokens[0] != "0") groupExists = true;
            if(tokens[1] == "U" && tokens.size() >= 5 && tokens[3] == user && tokens[0] != "0"){
                std::cout << "Error: Usuario ya existe\n"; file.close(); return;
            }
            int uid = std::stoi(tokens[0]);
            if(uid > maxId) maxId = uid;
        }
    }

    if(!groupExists){ std::cout << "Error: Grupo no existe\n"; file.close(); return; }

    content += std::to_string(maxId + 1) + ",U," + grp + "," + user + "," + pass + "\n";
    if(!BlockManager::WriteFileContent(file, sb, partStart, usersInode, content)){ file.close(); return; }
    file.seekp(sb.s_inode_start + sizeof(Inode));
    file.write(reinterpret_cast<char*>(&usersInode), sizeof(Inode));
    file.close();
    std::cout << "Usuario creado correctamente\n";
}

void Rmusr(const std::string& user)
{
    if(!LoginManager::IsLogged()){ std::cout << "Error: Debe iniciar sesión\n"; return; }
    if(!LoginManager::IsRoot())  { std::cout << "Error: Solo root puede ejecutar rmusr\n"; return; }

    std::fstream file;
    SuperBlock sb{};
    int partStart = -1;
    if(!OpenAndFind(LoginManager::GetSessionId(), file, sb, partStart)) return;

    Inode usersInode{};
    file.seekg(sb.s_inode_start + sizeof(Inode));
    file.read(reinterpret_cast<char*>(&usersInode), sizeof(Inode));
    std::string content = BlockManager::ReadFileContent(file, sb, usersInode);

    std::stringstream ss(content);
    std::string line, newContent;
    bool userFound = false;

    while(std::getline(ss, line)){
        if(line.empty()) continue;
        std::vector<std::string> tokens;
        std::stringstream ls(line); std::string token;
        while(std::getline(ls, token, ',')) tokens.push_back(token);
        if(tokens.size() >= 5 && tokens[1] == "U" && tokens[3] == user && tokens[0] != "0"){
            tokens[0] = "0"; userFound = true;
        }
        for(size_t i = 0; i < tokens.size(); i++){
            newContent += tokens[i];
            if(i < tokens.size()-1) newContent += ",";
        }
        newContent += "\n";
    }

    if(!userFound){ std::cout << "Error: Usuario no existe\n"; file.close(); return; }
    if(!BlockManager::WriteFileContent(file, sb, partStart, usersInode, newContent)){ file.close(); return; }
    file.seekp(sb.s_inode_start + sizeof(Inode));
    file.write(reinterpret_cast<char*>(&usersInode), sizeof(Inode));
    file.close();
    std::cout << "Usuario eliminado correctamente\n";
}

} // namespace UserManager