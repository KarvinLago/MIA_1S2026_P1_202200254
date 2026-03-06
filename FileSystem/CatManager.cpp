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
