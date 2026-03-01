// #include "UserManager.h"
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

// namespace UserManager {

// void Mkusr(const std::string& user,
//            const std::string& pass,
//            const std::string& grp)
// {
//     // 🔹 Validar sesión
//     if(!LoginManager::IsLogged()){
//         std::cout << "Error: Debe iniciar sesión\n";
//         return;
//     }

//     // 🔹 Solo root
//     if(!LoginManager::IsRoot()){
//         std::cout << "Error: Solo root puede ejecutar mkusr\n";
//         return;
//     }

//     // 🔹 Longitud máxima
//     if(user.size() > 10 || pass.size() > 10 || grp.size() > 10){
//         std::cout << "Error: user, pass y grp máximo 10 caracteres\n";
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

//     // 🔹 Localizar partición
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
//     bool groupExists = false;

//     while(std::getline(ss, line)){
//         if(line.empty()) continue;

//         std::vector<std::string> tokens;
//         std::stringstream ls(line);
//         std::string token;

//         while(std::getline(ls, token, ',')){
//             tokens.push_back(token);
//         }

//         if(tokens.size() >= 3){

//             // 🔹 Verificar grupo existente y no eliminado
//             if(tokens[1] == "G" && tokens[2] == grp && tokens[0] != "0"){
//                 groupExists = true;
//             }

//             // 🔹 Verificar usuario duplicado
//             if(tokens[1] == "U" && tokens.size() >= 5){
//                 if(tokens[3] == user && tokens[0] != "0"){
//                     std::cout << "Error: Usuario ya existe\n";
//                     file.close();
//                     return;
//                 }
//             }

//             int uid = std::stoi(tokens[0]);
//             if(uid > maxId) maxId = uid;
//         }
//     }

//     if(!groupExists){
//         std::cout << "Error: Grupo no existe\n";
//         file.close();
//         return;
//     }

//     int newId = maxId + 1;

//     std::string newLine =
//         std::to_string(newId) + ",U," +
//         grp + "," + user + "," + pass + "\n";

//     std::string newContent = content + newLine;

//     // 🔹 VALIDACIÓN DE TAMAÑO DEL BLOQUE (64 bytes)
//     if(newContent.size() > sizeof(FileBlock)){
//         std::cout << "Error: users.txt excede tamaño máximo (64 bytes)\n";
//         file.close();
//         return;
//     }

//     // 🔹 Escribir nuevamente el bloque
//     FileBlock newBlock{};
//     memset(newBlock.b_content, 0, sizeof(newBlock.b_content));
//     memcpy(newBlock.b_content,
//            newContent.c_str(),
//            newContent.size());

//     file.seekp(blockPos);
//     file.write(reinterpret_cast<char*>(&newBlock), sizeof(FileBlock));

//     // 🔹 Actualizar tamaño del inodo
//     usersInode.i_size = newContent.size();
//     file.seekp(sb.s_inode_start + sizeof(Inode));
//     file.write(reinterpret_cast<char*>(&usersInode), sizeof(Inode));

//     file.close();

//     std::cout << "Usuario creado correctamente\n";
// }

// void Rmusr(const std::string& user)
// {
//     // 🔹 Validar sesión
//     if(!LoginManager::IsLogged()){
//         std::cout << "Error: Debe iniciar sesión\n";
//         return;
//     }

//     // 🔹 Solo root
//     if(!LoginManager::IsRoot()){
//         std::cout << "Error: Solo root puede ejecutar rmusr\n";
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

//     // 🔹 Localizar partición
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
//     bool userFound = false;

//     while(std::getline(ss, line)){
//         if(line.empty()){
//             newContent += "\n";
//             continue;
//         }

//         std::vector<std::string> tokens;
//         std::stringstream ls(line);
//         std::string token;

//         while(std::getline(ls, token, ',')){
//             tokens.push_back(token);
//         }

//         if(tokens.size() >= 5 &&
//            tokens[1] == "U" &&
//            tokens[3] == user &&
//            tokens[0] != "0")
//         {
//             tokens[0] = "0"; // marcar eliminado
//             userFound = true;
//         }

//         // reconstruir línea
//         for(size_t i = 0; i < tokens.size(); i++){
//             newContent += tokens[i];
//             if(i < tokens.size()-1)
//                 newContent += ",";
//         }
//         newContent += "\n";
//     }

//     if(!userFound){
//         std::cout << "Error: Usuario no existe\n";
//         file.close();
//         return;
//     }

//     // 🔹 Validación de tamaño
//     if(newContent.size() > sizeof(usersBlock.b_content)){
//         std::cout << "Error: users.txt excede tamaño máximo (64 bytes)\n";
//         file.close();
//         return;
//     }

//     FileBlock newBlock{};
//     memset(newBlock.b_content, 0, sizeof(newBlock.b_content));
//     memcpy(newBlock.b_content, newContent.c_str(), newContent.size());

//     file.seekp(blockPos);
//     file.write(reinterpret_cast<char*>(&newBlock), sizeof(FileBlock));

//     usersInode.i_size = newContent.size();
//     file.seekp(sb.s_inode_start + sizeof(Inode));
//     file.write(reinterpret_cast<char*>(&usersInode), sizeof(Inode));

//     file.close();

//     std::cout << "Usuario eliminado correctamente\n";
// }

// }


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
#include <algorithm>

extern std::vector<MountedPartition> mountedList;

namespace UserManager {

void Mkusr(const std::string& user,
           const std::string& pass,
           const std::string& grp)
{
    if(!LoginManager::IsLogged()){
        std::cout << "Error: Debe iniciar sesión\n";
        return;
    }

    if(!LoginManager::IsRoot()){
        std::cout << "Error: Solo root puede ejecutar mkusr\n";
        return;
    }

    if(user.size() > 10 || pass.size() > 10 || grp.size() > 10){
        std::cout << "Error: user, pass y grp máximo 10 caracteres\n";
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
    int maxId = 0;
    bool groupExists = false;

    while(std::getline(ss, line)){
        if(line.empty()) continue;

        std::vector<std::string> tokens;
        std::stringstream ls(line);
        std::string token;

        while(std::getline(ls, token, ',')){
            tokens.push_back(token);
        }

        if(tokens.size() >= 3){
            if(tokens[1] == "G" && tokens[2] == grp && tokens[0] != "0"){
                groupExists = true;
            }

            if(tokens[1] == "U" && tokens.size() >= 5){
                if(tokens[3] == user && tokens[0] != "0"){
                    std::cout << "Error: Usuario ya existe\n";
                    file.close();
                    return;
                }
            }

            int uid = std::stoi(tokens[0]);
            if(uid > maxId) maxId = uid;
        }
    }

    if(!groupExists){
        std::cout << "Error: Grupo no existe\n";
        file.close();
        return;
    }

    int newId = maxId + 1;

    content += std::to_string(newId) + ",U," +
               grp + "," + user + "," + pass + "\n";

    if(!BlockManager::WriteFileContent(file, sb, partition->part_start, usersInode, content)){
        file.close();
        return;
    }

    file.seekp(sb.s_inode_start + sizeof(Inode));
    file.write(reinterpret_cast<char*>(&usersInode), sizeof(Inode));

    file.close();

    std::cout << "Usuario creado correctamente\n";
}

void Rmusr(const std::string& user)
{
    if(!LoginManager::IsLogged()){
        std::cout << "Error: Debe iniciar sesión\n";
        return;
    }

    if(!LoginManager::IsRoot()){
        std::cout << "Error: Solo root puede ejecutar rmusr\n";
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
    bool userFound = false;

    while(std::getline(ss, line)){
        if(line.empty()) continue;

        std::vector<std::string> tokens;
        std::stringstream ls(line);
        std::string token;

        while(std::getline(ls, token, ',')){
            tokens.push_back(token);
        }

        if(tokens.size() >= 5 &&
           tokens[1] == "U" &&
           tokens[3] == user &&
           tokens[0] != "0")
        {
            tokens[0] = "0";
            userFound = true;
        }

        for(size_t i = 0; i < tokens.size(); i++){
            newContent += tokens[i];
            if(i < tokens.size()-1)
                newContent += ",";
        }
        newContent += "\n";
    }

    if(!userFound){
        std::cout << "Error: Usuario no existe\n";
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

    std::cout << "Usuario eliminado correctamente\n";
}

}