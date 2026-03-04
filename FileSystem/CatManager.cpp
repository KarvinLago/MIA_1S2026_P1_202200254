// #include "CatManager.h"
// #include "BlockManager.h"
// #include "../Auth/LoginManager.h"
// #include "../Mount/MountManager.h"
// #include "../Structs/Structs.h"
// #include "../Utilities/FileUtils.h"
// #include <iostream>
// #include <sstream>
// #include <vector>
// #include <cstring>

// extern std::vector<MountedPartition> mountedList;

// namespace CatManager {

// // ======================================================
// // ================= SPLIT PATH =========================
// // Divide "/home/user/docs/a.txt" en ["home","user","docs","a.txt"]
// // ======================================================

// static std::vector<std::string> SplitPath(const std::string& path)
// {
//     std::vector<std::string> parts;
//     std::stringstream ss(path);
//     std::string token;

//     while(std::getline(ss, token, '/'))
//     {
//         if(!token.empty())
//             parts.push_back(token);
//     }

//     return parts;
// }

// // ======================================================
// // ================= FIND INODE =========================
// // Navega el árbol de directorios EXT2 y retorna el índice
// // del inodo correspondiente a la ruta dada.
// // Retorna -1 si no se encuentra.
// // ======================================================

// static int FindInodeByPath(std::fstream& file,
//                            SuperBlock& sb,
//                            const std::string& path)
// {
//     std::vector<std::string> parts = SplitPath(path);

//     if(parts.empty())
//         return 0; // raíz

//     int inodeSize  = sizeof(Inode);
//     int blockSize  = sizeof(FileBlock);

//     // Empezamos desde el inodo raíz (índice 0)
//     int currentInodeIndex = 0;

//     for(const std::string& part : parts)
//     {
//         // Leer inodo actual
//         Inode currentInode{};
//         int inodePos = sb.s_inode_start + (currentInodeIndex * inodeSize);

//         file.seekg(inodePos);
//         file.read(reinterpret_cast<char*>(&currentInode), inodeSize);

//         // Debe ser carpeta (type == 0) para poder navegar
//         if(currentInode.i_type != 0)
//         {
//             // Es un archivo, no una carpeta — ruta inválida
//             return -1;
//         }

//         bool found = false;

//         // Buscar en los bloques directos del inodo carpeta
//         for(int b = 0; b < 12 && !found; b++)
//         {
//             if(currentInode.i_block[b] == -1) break;

//             FolderBlock folder{};
//             int blockPos = sb.s_block_start + (currentInode.i_block[b] * blockSize);

//             file.seekg(blockPos);
//             file.read(reinterpret_cast<char*>(&folder), sizeof(FolderBlock));

//             for(int e = 0; e < 4 && !found; e++)
//             {
//                 if(folder.b_content[e].b_inodo == -1) continue;
//                 if(folder.b_content[e].b_name[0] == '\0') continue;

//                 std::string entryName(folder.b_content[e].b_name);

//                 // Ignorar . y ..
//                 if(entryName == "." || entryName == "..") continue;

//                 if(entryName == part)
//                 {
//                     currentInodeIndex = folder.b_content[e].b_inodo;
//                     found = true;
//                 }
//             }
//         }

//         if(!found)
//             return -1; // No encontrado
//     }

//     return currentInodeIndex;
// }

// // ======================================================
// // ================= CHECK READ PERMISSION ==============
// // Verifica si el usuario actual tiene permiso de lectura
// // sobre el inodo dado.
// // Permisos UGO en octal: 664 = rw-rw-r--
// // ======================================================

// static bool HasReadPermission(std::fstream& file,
//                               SuperBlock& sb,
//                               Inode& inode)
// {
//     // root siempre tiene permiso
//     if(LoginManager::IsRoot())
//         return true;

//     // Obtener uid y gid del usuario actual
//     int sessionUid = LoginManager::GetSessionUid();
//     int sessionGid = LoginManager::GetSessionGid();

//     int perm = inode.i_perm; // ej: 664

//     // Extraer dígitos UGO
//     int permU = (perm / 100) % 10; // propietario
//     int permG = (perm / 10)  % 10; // grupo
//     int permO = (perm)       % 10; // otros

//     // 4 = lectura, 6 = lectura+escritura, 7 = todo
//     auto canRead = [](int p) { return (p == 4 || p == 5 || p == 6 || p == 7); };

//     if(inode.i_uid == sessionUid)
//         return canRead(permU);

//     if(inode.i_gid == sessionGid)
//         return canRead(permG);

//     return canRead(permO);
// }

// // ======================================================
// // ================= CAT ================================
// // ======================================================

// void Cat(const std::vector<std::string>& paths)
// {
//     if(!LoginManager::IsLogged())
//     {
//         std::cout << "Error: Debe iniciar sesión\n";
//         return;
//     }

//     std::string id = LoginManager::GetSessionId();

//     MountedPartition* mounted = nullptr;
//     for(auto& m : mountedList)
//     {
//         if(m.id == id)
//         {
//             mounted = &m;
//             break;
//         }
//     }

//     if(!mounted)
//     {
//         std::cout << "Error: Partición no encontrada\n";
//         return;
//     }

//     auto file = FileUtils::OpenFile(mounted->path);
//     if(!file.is_open())
//     {
//         std::cout << "Error: No se pudo abrir el disco\n";
//         return;
//     }

//     // Leer MBR y localizar partición
//     MBR mbr{};
//     file.seekg(0);
//     file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

//     Partition* partition = nullptr;
//     for(int i = 0; i < 4; i++)
//     {
//         if(mbr.mbr_partitions[i].part_s > 0 &&
//            std::string(mbr.mbr_partitions[i].part_name) == mounted->name)
//         {
//             partition = &mbr.mbr_partitions[i];
//             break;
//         }
//     }

//     if(!partition)
//     {
//         std::cout << "Error: Partición no encontrada en disco\n";
//         file.close();
//         return;
//     }

//     SuperBlock sb{};
//     file.seekg(partition->part_start);
//     file.read(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));

//     // Procesar cada archivo en orden
//     bool first = true;
//     for(const std::string& path : paths)
//     {
//         int inodeIndex = FindInodeByPath(file, sb, path);

//         if(inodeIndex == -1)
//         {
//             std::cout << "Error: Archivo no encontrado: " << path << "\n";
//             continue;
//         }

//         // Leer inodo
//         Inode inode{};
//         int inodePos = sb.s_inode_start + (inodeIndex * sizeof(Inode));

//         file.seekg(inodePos);
//         file.read(reinterpret_cast<char*>(&inode), sizeof(Inode));

//         // Verificar que sea archivo
//         if(inode.i_type != 1)
//         {
//             std::cout << "Error: " << path << " es una carpeta, no un archivo\n";
//             continue;
//         }

//         // Verificar permiso de lectura
//         if(!HasReadPermission(file, sb, inode))
//         {
//             std::cout << "Error: Sin permiso de lectura: " << path << "\n";
//             continue;
//         }

//         // Leer contenido
//         std::string content = BlockManager::ReadFileContent(file, sb, inode);

//         // Separador entre archivos
//         if(!first)
//             std::cout << "\n";

//         std::cout << content;
//         first = false;
//     }

//     file.close();
// }

// } // namespace CatManager



#include "CatManager.h"
#include "BlockManager.h"
#include "../Auth/LoginManager.h"
#include "../Mount/MountManager.h"
#include "../Structs/Structs.h"
#include "../Utilities/FileUtils.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <cstring>

extern std::vector<MountedPartition> mountedList;

static std::vector<std::string> SplitPath(const std::string& path)
{
    std::vector<std::string> parts;
    std::stringstream ss(path); std::string token;
    while(std::getline(ss, token, '/')) if(!token.empty()) parts.push_back(token);
    return parts;
}

static int FindInodeByPath(std::fstream& file, SuperBlock& sb, const std::string& path)
{
    std::vector<std::string> parts = SplitPath(path);
    if(parts.empty()) return 0;
    int blockSize = sizeof(FileBlock);
    int cur = 0;
    for(const std::string& part : parts){
        Inode inode{};
        file.seekg(sb.s_inode_start + (cur * sizeof(Inode)));
        file.read(reinterpret_cast<char*>(&inode), sizeof(Inode));
        if(inode.i_type != 0) return -1;
        bool found = false;
        for(int b = 0; b < 12 && !found; b++){
            if(inode.i_block[b] == -1) break;
            FolderBlock fb{};
            file.seekg(sb.s_block_start + (inode.i_block[b] * blockSize));
            file.read(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));
            for(int e = 0; e < 4 && !found; e++){
                if(fb.b_content[e].b_inodo == -1) continue;
                if(fb.b_content[e].b_name[0] == '\0') continue;
                std::string name(fb.b_content[e].b_name);
                if(name == "." || name == "..") continue;
                if(name == part){ cur = fb.b_content[e].b_inodo; found = true; }
            }
        }
        if(!found) return -1;
    }
    return cur;
}

static bool HasReadPermission(Inode& inode)
{
    if(LoginManager::IsRoot()) return true;
    int perm = inode.i_perm;
    int uid  = LoginManager::GetSessionUid();
    int gid  = LoginManager::GetSessionGid();
    int bit;
    if(inode.i_uid == uid)      bit = (perm / 100) % 10;
    else if(inode.i_gid == gid) bit = (perm / 10)  % 10;
    else                        bit =  perm         % 10;
    return (bit == 4 || bit == 5 || bit == 6 || bit == 7);
}

namespace CatManager {

void Cat(const std::vector<std::string>& paths)
{
    if(!LoginManager::IsLogged()){ std::cout << "Error: Debe iniciar sesión\n"; return; }

    std::string id = LoginManager::GetSessionId();
    MountedPartition* mounted = nullptr;
    for(auto& m : mountedList) if(m.id == id){ mounted = &m; break; }
    if(!mounted){ std::cout << "Error: Partición no encontrada\n"; return; }

    auto file = FileUtils::OpenFile(mounted->path);
    if(!file.is_open()){ std::cout << "Error: No se pudo abrir el disco\n"; return; }

    int partStart = -1, partSize = -1;
    if(!FileUtils::FindPartition(file, mounted->name, partStart, partSize)){
        std::cout << "Error: Partición no encontrada en disco\n"; file.close(); return;
    }

    SuperBlock sb{};
    file.seekg(partStart);
    file.read(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));

    bool first = true;
    for(const std::string& path : paths){
        int inodeIdx = FindInodeByPath(file, sb, path);
        if(inodeIdx == -1){ std::cout << "Error: Archivo no encontrado: " << path << "\n"; continue; }

        Inode inode{};
        file.seekg(sb.s_inode_start + (inodeIdx * sizeof(Inode)));
        file.read(reinterpret_cast<char*>(&inode), sizeof(Inode));

        if(inode.i_type != 1){ std::cout << "Error: " << path << " es una carpeta\n"; continue; }
        if(!HasReadPermission(inode)){ std::cout << "Error: Sin permiso de lectura: " << path << "\n"; continue; }

        std::string content = BlockManager::ReadFileContent(file, sb, inode);
        if(!first) std::cout << "\n";
        std::cout << content;
        first = false;
    }
    file.close();
}

} // namespace CatManager