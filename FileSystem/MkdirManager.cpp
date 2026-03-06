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
