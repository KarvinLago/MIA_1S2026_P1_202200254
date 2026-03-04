#include "GroupManager.h"
#include "../Auth/LoginManager.h"
#include "../Mount/MountManager.h"
#include "../Structs/Structs.h"
#include "../Utilities/FileUtils.h"
#include "../FileSystem/BlockManager.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <cstring>
#include <algorithm>

extern std::vector<MountedPartition> mountedList;

// Helper para abrir disco y localizar partición activa
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

namespace GroupManager {

void Mkgrp(const std::string& name)
{
    if(!LoginManager::IsLogged()){ std::cout << "Error: Debe iniciar sesión\n"; return; }
    if(!LoginManager::IsRoot())  { std::cout << "Error: Solo root puede ejecutar mkgrp\n"; return; }

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

    while(std::getline(ss, line)){
        if(line.empty()) continue;
        std::vector<std::string> tokens;
        std::stringstream ls(line); std::string token;
        while(std::getline(ls, token, ',')) tokens.push_back(token);

        if(tokens.size() >= 1 && tokens[0] != "0"){
            int cur = std::stoi(tokens[0]);
            if(cur > maxId) maxId = cur;
        }
        if(tokens.size() >= 3 && tokens[1] == "G" && tokens[0] != "0" && tokens[2] == name){
            std::cout << "Error: El grupo ya existe\n"; file.close(); return;
        }
    }

    content += std::to_string(maxId + 1) + ",G," + name + "\n";
    if(!BlockManager::WriteFileContent(file, sb, partStart, usersInode, content)){ file.close(); return; }
    file.seekp(sb.s_inode_start + sizeof(Inode));
    file.write(reinterpret_cast<char*>(&usersInode), sizeof(Inode));
    file.close();
    std::cout << "Grupo creado correctamente\n";
}

void Rmgrp(const std::string& name)
{
    if(!LoginManager::IsLogged()){ std::cout << "Error: Debe iniciar sesión\n"; return; }
    if(!LoginManager::IsRoot())  { std::cout << "Error: Solo root puede ejecutar rmgrp\n"; return; }

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
    bool found = false;

    while(std::getline(ss, line)){
        if(line.empty()) continue;
        std::vector<std::string> tokens;
        std::stringstream ls(line); std::string token;
        while(std::getline(ls, token, ',')) tokens.push_back(token);
        if(tokens.size() >= 3 && tokens[1] == "G" && tokens[2] == name && tokens[0] != "0"){
            tokens[0] = "0"; found = true;
        }
        for(size_t i = 0; i < tokens.size(); i++){
            newContent += tokens[i];
            if(i < tokens.size()-1) newContent += ",";
        }
        newContent += "\n";
    }

    if(!found){ std::cout << "Error: Grupo no encontrado\n"; file.close(); return; }
    if(!BlockManager::WriteFileContent(file, sb, partStart, usersInode, newContent)){ file.close(); return; }
    file.seekp(sb.s_inode_start + sizeof(Inode));
    file.write(reinterpret_cast<char*>(&usersInode), sizeof(Inode));
    file.close();
    std::cout << "Grupo eliminado correctamente\n";
}

void Chgrp(const std::string& user, const std::string& grp)
{
    if(!LoginManager::IsLogged()){ std::cout << "Error: Debe iniciar sesión\n"; return; }
    if(!LoginManager::IsRoot())  { std::cout << "Error: Solo root puede ejecutar chgrp\n"; return; }

    std::fstream file;
    SuperBlock sb{};
    int partStart = -1;
    if(!OpenAndFind(LoginManager::GetSessionId(), file, sb, partStart)) return;

    Inode usersInode{};
    file.seekg(sb.s_inode_start + sizeof(Inode));
    file.read(reinterpret_cast<char*>(&usersInode), sizeof(Inode));
    std::string content = BlockManager::ReadFileContent(file, sb, usersInode);

    // Verificar que el grupo existe
    bool groupExists = false;
    { std::stringstream ss(content); std::string line;
      while(std::getline(ss, line)){
          if(line.empty()) continue;
          std::vector<std::string> t; std::stringstream ls(line); std::string tok;
          while(std::getline(ls, tok, ',')) t.push_back(tok);
          if(t.size() >= 3 && t[1] == "G" && t[0] != "0" && t[2] == grp){ groupExists = true; break; }
      }
    }
    if(!groupExists){ std::cout << "Error: Grupo '" << grp << "' no existe\n"; file.close(); return; }

    std::stringstream ss(content);
    std::string line, newContent;
    bool userFound = false;

    while(std::getline(ss, line)){
        if(line.empty()) continue;
        std::vector<std::string> tokens;
        std::stringstream ls(line); std::string token;
        while(std::getline(ls, token, ',')) tokens.push_back(token);
        if(tokens.size() == 5 && tokens[1] == "U" && tokens[0] != "0" && tokens[3] == user){
            tokens[2] = grp; userFound = true;
        }
        for(size_t i = 0; i < tokens.size(); i++){
            newContent += tokens[i];
            if(i < tokens.size()-1) newContent += ",";
        }
        newContent += "\n";
    }

    if(!userFound){ std::cout << "Error: Usuario '" << user << "' no existe\n"; file.close(); return; }
    if(!BlockManager::WriteFileContent(file, sb, partStart, usersInode, newContent)){ file.close(); return; }
    file.seekp(sb.s_inode_start + sizeof(Inode));
    file.write(reinterpret_cast<char*>(&usersInode), sizeof(Inode));
    file.close();
    std::cout << "Grupo del usuario '" << user << "' cambiado a '" << grp << "' correctamente\n";
}

} // namespace GroupManager