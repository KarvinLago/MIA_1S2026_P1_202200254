// #include "GroupManager.h"
// #include "../Auth/LoginManager.h"
// #include "../Mount/MountManager.h"
// #include "../Structs/Structs.h"
// #include "../Utilities/FileUtils.h"
// #include <iostream>
// #include <sstream>
// #include <vector>
// #include <cstring>
// #include <algorithm>

// extern std::vector<MountedPartition> mountedList;

// namespace GroupManager {

// // =====================================================
// // ===================== MKGRP ==========================
// // =====================================================

// void Mkgrp(const std::string& name)
// {
//     if(!LoginManager::IsLogged()){
//         std::cout << "Error: Debe iniciar sesión\n";
//         return;
//     }

//     if(!LoginManager::IsRoot()){
//         std::cout << "Error: Solo root puede ejecutar mkgrp\n";
//         return;
//     }

//     std::string id = LoginManager::GetSessionId();

//     MountedPartition* mounted = nullptr;
//     for(auto& m : mountedList){
//         if(m.id == id){
//             mounted = &m;
//             break;
//         }
//     }

//     if(mounted == nullptr){
//         std::cout << "Error: Partición no encontrada\n";
//         return;
//     }

//     auto file = FileUtils::OpenFile(mounted->path);
//     if(!file.is_open()){
//         std::cout << "Error: No se pudo abrir el disco\n";
//         return;
//     }

//     MBR mbr{};
//     file.seekg(0);
//     file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

//     Partition* partition = nullptr;
//     for(int i=0;i<4;i++){
//         if(mbr.mbr_partitions[i].part_s > 0 &&
//            std::string(mbr.mbr_partitions[i].part_name) == mounted->name){
//             partition = &mbr.mbr_partitions[i];
//             break;
//         }
//     }

//     if(!partition){
//         std::cout << "Error: Partición no encontrada\n";
//         file.close();
//         return;
//     }

//     SuperBlock sb{};
//     file.seekg(partition->part_start);
//     file.read(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));

//     Inode usersInode{};
//     file.seekg(sb.s_inode_start + sizeof(Inode));
//     file.read(reinterpret_cast<char*>(&usersInode), sizeof(Inode));

//     int blockIndex = usersInode.i_block[0];
//     int blockPos = sb.s_block_start + (blockIndex * sizeof(FileBlock));

//     FileBlock usersBlock{};
//     file.seekg(blockPos);
//     file.read(reinterpret_cast<char*>(&usersBlock), sizeof(FileBlock));

//     std::string content(usersBlock.b_content);

//     std::stringstream ss(content);
//     std::string line;
//     int maxId = 0;

//     while(std::getline(ss, line)){
//         if(line.empty()) continue;

//         std::vector<std::string> tokens;
//         std::stringstream ls(line);
//         std::string token;

//         while(std::getline(ls, token, ',')){
//             tokens.push_back(token);
//         }

//         if(tokens.size() >= 3 && tokens[1] == "G"){
//             int gid = std::stoi(tokens[0]);

//             if(tokens[2] == name && gid != 0){
//                 std::cout << "Error: El grupo ya existe\n";
//                 file.close();
//                 return;
//             }

//             if(gid > maxId) maxId = gid;
//         }
//     }

//     int newId = maxId + 1;
//     content += std::to_string(newId) + ",G," + name + "\n";

//     FileBlock newBlock{};
//     memset(newBlock.b_content, 0, sizeof(newBlock.b_content));
//     memcpy(newBlock.b_content, content.c_str(),
//            std::min(content.size(), sizeof(newBlock.b_content)));

//     file.seekp(blockPos);
//     file.write(reinterpret_cast<char*>(&newBlock), sizeof(FileBlock));

//     usersInode.i_size = content.size();
//     file.seekp(sb.s_inode_start + sizeof(Inode));
//     file.write(reinterpret_cast<char*>(&usersInode), sizeof(Inode));

//     file.close();

//     std::cout << "Grupo creado correctamente\n";
// }

// // =====================================================
// // ===================== RMGRP ==========================
// // =====================================================

// void Rmgrp(const std::string& name)
// {
//     if(!LoginManager::IsLogged()){
//         std::cout << "Error: Debe iniciar sesión\n";
//         return;
//     }

//     if(!LoginManager::IsRoot()){
//         std::cout << "Error: Solo root puede ejecutar rmgrp\n";
//         return;
//     }

//     std::string id = LoginManager::GetSessionId();

//     MountedPartition* mounted = nullptr;
//     for(auto& m : mountedList){
//         if(m.id == id){
//             mounted = &m;
//             break;
//         }
//     }

//     if(!mounted){
//         std::cout << "Error: Partición no encontrada\n";
//         return;
//     }

//     auto file = FileUtils::OpenFile(mounted->path);
//     if(!file.is_open()){
//         std::cout << "Error: No se pudo abrir el disco\n";
//         return;
//     }

//     MBR mbr{};
//     file.seekg(0);
//     file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

//     Partition* partition = nullptr;
//     for(int i=0;i<4;i++){
//         if(mbr.mbr_partitions[i].part_s > 0 &&
//            std::string(mbr.mbr_partitions[i].part_name) == mounted->name){
//             partition = &mbr.mbr_partitions[i];
//             break;
//         }
//     }

//     if(!partition){
//         std::cout << "Error: Partición no encontrada\n";
//         file.close();
//         return;
//     }

//     SuperBlock sb{};
//     file.seekg(partition->part_start);
//     file.read(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));

//     Inode usersInode{};
//     file.seekg(sb.s_inode_start + sizeof(Inode));
//     file.read(reinterpret_cast<char*>(&usersInode), sizeof(Inode));

//     int blockIndex = usersInode.i_block[0];
//     int blockPos = sb.s_block_start + (blockIndex * sizeof(FileBlock));

//     FileBlock usersBlock{};
//     file.seekg(blockPos);
//     file.read(reinterpret_cast<char*>(&usersBlock), sizeof(FileBlock));

//     std::string content(usersBlock.b_content);
//     std::stringstream ss(content);
//     std::string line;
//     std::string newContent;
//     bool found = false;

//     while(std::getline(ss, line)){
//         if(line.empty()) continue;

//         std::vector<std::string> tokens;
//         std::stringstream ls(line);
//         std::string token;

//         while(std::getline(ls, token, ',')){
//             tokens.push_back(token);
//         }

//         if(tokens.size() >= 3 && tokens[1] == "G"){
//             if(tokens[2] == name && tokens[0] != "0"){
//                 tokens[0] = "0";
//                 found = true;
//             }
//         }

//         if(tokens.size() >= 3){
//             newContent += tokens[0] + "," + tokens[1] + "," + tokens[2];
//             if(tokens.size() > 3){
//                 for(size_t i=3;i<tokens.size();i++)
//                     newContent += "," + tokens[i];
//             }
//             newContent += "\n";
//         }
//     }

//     if(!found){
//         std::cout << "Error: Grupo no encontrado\n";
//         file.close();
//         return;
//     }

//     FileBlock newBlock{};
//     memset(newBlock.b_content, 0, sizeof(newBlock.b_content));
//     memcpy(newBlock.b_content, newContent.c_str(),
//            std::min(newContent.size(), sizeof(newBlock.b_content)));

//     file.seekp(blockPos);
//     file.write(reinterpret_cast<char*>(&newBlock), sizeof(FileBlock));

//     usersInode.i_size = newContent.size();
//     file.seekp(sb.s_inode_start + sizeof(Inode));
//     file.write(reinterpret_cast<char*>(&usersInode), sizeof(Inode));

//     file.close();

//     std::cout << "Grupo eliminado correctamente\n";
// }

// }

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

namespace GroupManager {

// ===================== MKGRP ==========================

void Mkgrp(const std::string& name)
{
    if(!LoginManager::IsLogged()){
        std::cout << "Error: Debe iniciar sesión\n";
        return;
    }

    if(!LoginManager::IsRoot()){
        std::cout << "Error: Solo root puede ejecutar mkgrp\n";
        return;
    }

    std::string id = LoginManager::GetSessionId();

    MountedPartition* mounted = nullptr;
    for(auto& m : mountedList){
        if(m.id == id){
            mounted = &m;
            break;
        }
    }

    if(!mounted){
        std::cout << "Error: Partición no encontrada\n";
        return;
    }

    auto file = FileUtils::OpenFile(mounted->path);
    if(!file.is_open()){
        std::cout << "Error: No se pudo abrir el disco\n";
        return;
    }

    // localizar partición
    MBR mbr{};
    file.seekg(0);
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

    Partition* partition = nullptr;
    for(int i=0;i<4;i++){
        if(mbr.mbr_partitions[i].part_s > 0 &&
           std::string(mbr.mbr_partitions[i].part_name) == mounted->name){
            partition = &mbr.mbr_partitions[i];
            break;
        }
    }

    if(!partition){
        std::cout << "Error: Partición no encontrada\n";
        file.close();
        return;
    }

    SuperBlock sb{};
    file.seekg(partition->part_start);
    file.read(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));

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
        std::stringstream ls(line);
        std::string token;

        while(std::getline(ls, token, ',')){
            tokens.push_back(token);
        }

        if(tokens.size() >= 3 && tokens[1] == "G"){
            int gid = std::stoi(tokens[0]);

            if(tokens[2] == name && gid != 0){
                std::cout << "Error: El grupo ya existe\n";
                file.close();
                return;
            }

            if(gid > maxId) maxId = gid;
        }
    }

    int newId = maxId + 1;
    content += std::to_string(newId) + ",G," + name + "\n";

    if(!BlockManager::WriteFileContent(file, sb, partition->part_start, usersInode, content)){
        file.close();
        return;
    }

    file.seekp(sb.s_inode_start + sizeof(Inode));
    file.write(reinterpret_cast<char*>(&usersInode), sizeof(Inode));

    file.close();

    std::cout << "Grupo creado correctamente\n";
}

// ===================== RMGRP ==========================

void Rmgrp(const std::string& name)
{
    if(!LoginManager::IsLogged()){
        std::cout << "Error: Debe iniciar sesión\n";
        return;
    }

    if(!LoginManager::IsRoot()){
        std::cout << "Error: Solo root puede ejecutar rmgrp\n";
        return;
    }

    std::string id = LoginManager::GetSessionId();

    MountedPartition* mounted = nullptr;
    for(auto& m : mountedList){
        if(m.id == id){
            mounted = &m;
            break;
        }
    }

    if(!mounted){
        std::cout << "Error: Partición no encontrada\n";
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
        if(mbr.mbr_partitions[i].part_s > 0 &&
           std::string(mbr.mbr_partitions[i].part_name) == mounted->name){
            partition = &mbr.mbr_partitions[i];
            break;
        }
    }

    if(!partition){
        std::cout << "Error: Partición no encontrada\n";
        file.close();
        return;
    }

    SuperBlock sb{};
    file.seekg(partition->part_start);
    file.read(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));

    Inode usersInode{};
    file.seekg(sb.s_inode_start + sizeof(Inode));
    file.read(reinterpret_cast<char*>(&usersInode), sizeof(Inode));

    std::string content = BlockManager::ReadFileContent(file, sb, usersInode);

    std::stringstream ss(content);
    std::string line;
    std::string newContent;
    bool found = false;

    while(std::getline(ss, line)){
        if(line.empty()) continue;

        std::vector<std::string> tokens;
        std::stringstream ls(line);
        std::string token;

        while(std::getline(ls, token, ',')){
            tokens.push_back(token);
        }

        if(tokens.size() >= 3 && tokens[1] == "G"){
            if(tokens[2] == name && tokens[0] != "0"){
                tokens[0] = "0";
                found = true;
            }
        }

        for(size_t i = 0; i < tokens.size(); i++){
            newContent += tokens[i];
            if(i < tokens.size()-1)
                newContent += ",";
        }
        newContent += "\n";
    }

    if(!found){
        std::cout << "Error: Grupo no encontrado\n";
        file.close();
        return;
    }

    if(!BlockManager::WriteFileContent(file, sb, partition->part_start, usersInode, newContent)){
        file.close();
        return;
    }

    file.seekp(sb.s_inode_start + sizeof(Inode));
    file.write(reinterpret_cast<char*>(&usersInode), sizeof(Inode));

    file.close();

    std::cout << "Grupo eliminado correctamente\n";
}

}