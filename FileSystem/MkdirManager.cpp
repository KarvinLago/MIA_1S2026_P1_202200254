// #include "MkdirManager.h"
// #include "BlockManager.h"
// #include "../Auth/LoginManager.h"
// #include "../Mount/MountManager.h"
// #include "../Structs/Structs.h"
// #include "../Utilities/FileUtils.h"
// #include <iostream>
// #include <sstream>
// #include <vector>
// #include <cstring>
// #include <ctime>

// extern std::vector<MountedPartition> mountedList;

// namespace MkdirManager {

// // ======================================================
// // ================= SPLIT PATH =========================
// // ======================================================

// static std::vector<std::string> SplitPath(const std::string& path)
// {
//     std::vector<std::string> parts;
//     std::stringstream ss(path);
//     std::string token;
//     while(std::getline(ss, token, '/'))
//         if(!token.empty()) parts.push_back(token);
//     return parts;
// }

// // ======================================================
// // ================= FIND INODE BY PATH =================
// // Retorna el índice del inodo de la ruta dada.
// // Retorna -1 si no existe.
// // ======================================================

// static int FindInodeByPath(std::fstream& file, SuperBlock& sb,
//                            const std::string& path)
// {
//     std::vector<std::string> parts = SplitPath(path);
//     if(parts.empty()) return 0; // raíz

//     int blockSize = sizeof(FileBlock);
//     int currentInodeIndex = 0;

//     for(const std::string& part : parts)
//     {
//         Inode current = BlockManager::ReadInode(file, sb, currentInodeIndex);

//         if(current.i_type != 0) return -1; // no es carpeta

//         bool found = false;

//         for(int b = 0; b < 12 && !found; b++)
//         {
//             if(current.i_block[b] == -1) break;

//             FolderBlock fb{};
//             file.seekg(sb.s_block_start + (current.i_block[b] * blockSize));
//             file.read(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));

//             for(int e = 0; e < 4 && !found; e++)
//             {
//                 if(fb.b_content[e].b_inodo == -1) continue;
//                 if(fb.b_content[e].b_name[0] == '\0') continue;

//                 std::string name(fb.b_content[e].b_name);
//                 if(name == "." || name == "..") continue;

//                 if(name == part)
//                 {
//                     currentInodeIndex = fb.b_content[e].b_inodo;
//                     found = true;
//                 }
//             }
//         }

//         if(!found) return -1;
//     }

//     return currentInodeIndex;
// }

// // ======================================================
// // ================= CREATE SINGLE DIR ==================
// // Crea UNA carpeta dentro del inodo padre dado.
// // Retorna el índice del nuevo inodo, o -1 si falla.
// // ======================================================

// static int CreateSingleDir(std::fstream& file, SuperBlock& sb,
//                            int partitionStart,
//                            int parentInodeIndex,
//                            const std::string& dirName)
// {
//     int blockSize = sizeof(FileBlock);

//     // Verificar permiso de escritura en carpeta padre
//     Inode parentInode = BlockManager::ReadInode(file, sb, parentInodeIndex);

//     if(!LoginManager::IsRoot())
//     {
//         int perm = parentInode.i_perm;
//         int uid  = LoginManager::GetSessionUid();
//         int gid  = LoginManager::GetSessionGid();

//         int permBit = (perm / 100) % 10; // default: otros
//         if(parentInode.i_uid == uid)      permBit = (perm / 100) % 10; // U
//         else if(parentInode.i_gid == gid) permBit = (perm / 10)  % 10; // G
//         else                              permBit = (perm)        % 10; // O

//         bool canWrite = (permBit == 2 || permBit == 3 ||
//                          permBit == 6 || permBit == 7);
//         if(!canWrite)
//         {
//             std::cout << "Error: Sin permiso de escritura en carpeta padre\n";
//             return -1;
//         }
//     }

//     // Asignar inodo nuevo
//     int newInodeIndex = BlockManager::AllocateInode(file, sb, partitionStart);
//     if(newInodeIndex == -1) return -1;

//     // Asignar bloque carpeta nuevo
//     int newBlockIndex = BlockManager::AllocateBlock(file, sb, partitionStart);
//     if(newBlockIndex == -1)
//     {
//         BlockManager::FreeInode(file, sb, partitionStart, newInodeIndex);
//         return -1;
//     }

//     // Crear FolderBlock con . y ..
//     FolderBlock fb{};
//     for(int i = 0; i < 4; i++) fb.b_content[i].b_inodo = -1;

//     strncpy(fb.b_content[0].b_name, ".", 11);
//     fb.b_content[0].b_inodo = newInodeIndex;

//     strncpy(fb.b_content[1].b_name, "..", 11);
//     fb.b_content[1].b_inodo = parentInodeIndex;

//     int blockPos = sb.s_block_start + (newBlockIndex * blockSize);
//     file.seekp(blockPos);
//     file.write(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));

//     // Crear inodo de la nueva carpeta
//     Inode newInode{};
//     newInode.i_uid  = LoginManager::GetSessionUid();
//     newInode.i_gid  = LoginManager::GetSessionGid();
//     newInode.i_size = 0;
//     newInode.i_atime = time(nullptr);
//     newInode.i_ctime = time(nullptr);
//     newInode.i_mtime = time(nullptr);
//     newInode.i_type  = 0; // carpeta
//     newInode.i_perm  = 664;
//     for(int i = 0; i < 15; i++) newInode.i_block[i] = -1;
//     newInode.i_block[0] = newBlockIndex;

//     BlockManager::WriteInode(file, sb, newInodeIndex, newInode);

//     // Agregar entrada en carpeta padre
//     parentInode = BlockManager::ReadInode(file, sb, parentInodeIndex);
//     if(!BlockManager::AddEntryToFolder(file, sb, partitionStart,
//                                        parentInode, parentInodeIndex,
//                                        dirName, newInodeIndex))
//     {
//         BlockManager::FreeInode(file, sb, partitionStart, newInodeIndex);
//         BlockManager::FreeBlock(file, sb, partitionStart, newBlockIndex);
//         return -1;
//     }

//     BlockManager::UpdateSuperBlock(file, partitionStart, sb);
//     return newInodeIndex;
// }

// // ======================================================
// // ================= MKDIR ==============================
// // ======================================================

// void Mkdir(const std::string& path, bool createParents)
// {
//     if(!LoginManager::IsLogged())
//     {
//         std::cout << "Error: Debe iniciar sesión\n";
//         return;
//     }

//     std::string id = LoginManager::GetSessionId();

//     MountedPartition* mounted = nullptr;
//     for(auto& m : mountedList)
//         if(m.id == id) { mounted = &m; break; }

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

//     std::vector<std::string> parts = SplitPath(path);

//     if(parts.empty())
//     {
//         std::cout << "Error: Ruta inválida\n";
//         file.close();
//         return;
//     }

//     if(!createParents)
//     {
//         // ── Modo normal: la carpeta padre debe existir ──
//         std::string parentPath = "";
//         for(int i = 0; i < (int)parts.size() - 1; i++)
//             parentPath += "/" + parts[i];

//         int parentInodeIndex = parentPath.empty() ? 0
//                                : FindInodeByPath(file, sb, parentPath);

//         if(parentInodeIndex == -1)
//         {
//             std::cout << "Error: Carpeta padre no existe. Use -p para crearla\n";
//             file.close();
//             return;
//         }

//         // Verificar que la carpeta no exista ya
//         if(FindInodeByPath(file, sb, path) != -1)
//         {
//             std::cout << "Error: La carpeta ya existe\n";
//             file.close();
//             return;
//         }

//         int result = CreateSingleDir(file, sb, partition->part_start,
//                                      parentInodeIndex, parts.back());
//         if(result != -1)
//             std::cout << "Carpeta creada correctamente: " << path << "\n";
//     }
//     else
//     {
//         // ── Modo -p: crear carpetas intermedias si no existen ──
//         int currentInodeIndex = 0; // empezar desde raíz
//         std::string currentPath = "";

//         for(const std::string& part : parts)
//         {
//             currentPath += "/" + part;

//             int existing = FindInodeByPath(file, sb, currentPath);

//             if(existing != -1)
//             {
//                 // Ya existe — verificar que sea carpeta
//                 Inode existingInode = BlockManager::ReadInode(file, sb, existing);
//                 if(existingInode.i_type != 0)
//                 {
//                     std::cout << "Error: " << currentPath
//                               << " existe pero no es una carpeta\n";
//                     file.close();
//                     return;
//                 }
//                 currentInodeIndex = existing;
//             }
//             else
//             {
//                 // No existe — crear
//                 int newIndex = CreateSingleDir(file, sb, partition->part_start,
//                                                currentInodeIndex, part);
//                 if(newIndex == -1)
//                 {
//                     file.close();
//                     return;
//                 }
//                 currentInodeIndex = newIndex;
//                 std::cout << "Carpeta creada: " << currentPath << "\n";
//             }
//         }
//     }

//     file.close();
// }

// } // namespace MkdirManager


#include "MkdirManager.h"
#include "BlockManager.h"
#include "../Auth/LoginManager.h"
#include "../Mount/MountManager.h"
#include "../Structs/Structs.h"
#include "../Utilities/FileUtils.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <cstring>
#include <ctime>

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
        Inode inode = BlockManager::ReadInode(file, sb, cur);
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

static int CreateSingleDir(std::fstream& file, SuperBlock& sb, int partStart,
                           int parentIdx, const std::string& dirName)
{
    int blockSize = sizeof(FileBlock);
    Inode parentInode = BlockManager::ReadInode(file, sb, parentIdx);

    if(!LoginManager::IsRoot()){
        int perm = parentInode.i_perm;
        int uid  = LoginManager::GetSessionUid();
        int gid  = LoginManager::GetSessionGid();
        int bit;
        if(parentInode.i_uid == uid)      bit = (perm / 100) % 10;
        else if(parentInode.i_gid == gid) bit = (perm / 10)  % 10;
        else                              bit =  perm         % 10;
        if(!(bit == 2 || bit == 3 || bit == 6 || bit == 7)){
            std::cout << "Error: Sin permiso de escritura en carpeta padre\n"; return -1;
        }
    }

    int newInodeIdx = BlockManager::AllocateInode(file, sb, partStart);
    if(newInodeIdx == -1) return -1;
    int newBlockIdx = BlockManager::AllocateBlock(file, sb, partStart);
    if(newBlockIdx == -1){ BlockManager::FreeInode(file, sb, partStart, newInodeIdx); return -1; }

    FolderBlock fb{};
    for(int i = 0; i < 4; i++) fb.b_content[i].b_inodo = -1;
    strncpy(fb.b_content[0].b_name, ".", 11);  fb.b_content[0].b_inodo = newInodeIdx;
    strncpy(fb.b_content[1].b_name, "..", 11); fb.b_content[1].b_inodo = parentIdx;
    file.seekp(sb.s_block_start + (newBlockIdx * blockSize));
    file.write(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));

    Inode newInode{};
    newInode.i_uid = LoginManager::GetSessionUid();
    newInode.i_gid = LoginManager::GetSessionGid();
    newInode.i_size = 0;
    newInode.i_atime = newInode.i_ctime = newInode.i_mtime = time(nullptr);
    newInode.i_type = 0; newInode.i_perm = 664;
    for(int i = 0; i < 15; i++) newInode.i_block[i] = -1;
    newInode.i_block[0] = newBlockIdx;
    BlockManager::WriteInode(file, sb, newInodeIdx, newInode);

    parentInode = BlockManager::ReadInode(file, sb, parentIdx);
    if(!BlockManager::AddEntryToFolder(file, sb, partStart, parentInode, parentIdx, dirName, newInodeIdx)){
        BlockManager::FreeInode(file, sb, partStart, newInodeIdx);
        BlockManager::FreeBlock(file, sb, partStart, newBlockIdx);
        return -1;
    }
    BlockManager::UpdateSuperBlock(file, partStart, sb);
    return newInodeIdx;
}

namespace MkdirManager {

void Mkdir(const std::string& path, bool createParents)
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

    std::vector<std::string> parts = SplitPath(path);
    if(parts.empty()){ std::cout << "Error: Ruta inválida\n"; file.close(); return; }

    if(!createParents){
        std::string parentPath = "";
        for(int i = 0; i < (int)parts.size()-1; i++) parentPath += "/" + parts[i];
        int parentIdx = parentPath.empty() ? 0 : FindInodeByPath(file, sb, parentPath);
        if(parentIdx == -1){ std::cout << "Error: Carpeta padre no existe. Use -p\n"; file.close(); return; }
        if(FindInodeByPath(file, sb, path) != -1){ std::cout << "Error: La carpeta ya existe\n"; file.close(); return; }
        int result = CreateSingleDir(file, sb, partStart, parentIdx, parts.back());
        if(result != -1) std::cout << "Carpeta creada correctamente: " << path << "\n";
    } else {
        int curIdx = 0;
        std::string curPath = "";
        for(const std::string& part : parts){
            curPath += "/" + part;
            int existing = FindInodeByPath(file, sb, curPath);
            if(existing != -1){
                Inode e = BlockManager::ReadInode(file, sb, existing);
                if(e.i_type != 0){ std::cout << "Error: " << curPath << " no es carpeta\n"; file.close(); return; }
                curIdx = existing;
            } else {
                int newIdx = CreateSingleDir(file, sb, partStart, curIdx, part);
                if(newIdx == -1){ file.close(); return; }
                curIdx = newIdx;
                std::cout << "Carpeta creada: " << curPath << "\n";
            }
        }
    }
    file.close();
}

} // namespace MkdirManager