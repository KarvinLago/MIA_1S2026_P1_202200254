#include "MkfileManager.h"
#include "BlockManager.h"
#include "MkdirManager.h"
#include "../Auth/LoginManager.h"
#include "../Mount/MountManager.h"
#include "../Structs/Structs.h"
#include "../Utilities/FileUtils.h"
#include <iostream>
#include <fstream>
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

static bool HasWritePermission(Inode& inode)
{
    if(LoginManager::IsRoot()) return true;
    int perm = inode.i_perm;
    int uid  = LoginManager::GetSessionUid();
    int gid  = LoginManager::GetSessionGid();
    int bit;
    if(inode.i_uid == uid)      bit = (perm / 100) % 10;
    else if(inode.i_gid == gid) bit = (perm / 10)  % 10;
    else                        bit =  perm         % 10;
    return (bit == 2 || bit == 3 || bit == 6 || bit == 7);
}

namespace MkfileManager {

void Mkfile(const std::string& path, bool createParents, int size, const std::string& cont)
{
    if(!LoginManager::IsLogged()){ std::cout << "Error: Debe iniciar sesión\n"; return; }
    if(size < 0){ std::cout << "Error: El tamaño no puede ser negativo\n"; return; }

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

    std::string fileName = parts.back();
    std::string parentPath = "";
    for(int i = 0; i < (int)parts.size()-1; i++) parentPath += "/" + parts[i];

    // Crear carpetas padre si no existen y -r
    if(!parentPath.empty() && FindInodeByPath(file, sb, parentPath) == -1){
        if(!createParents){
            std::cout << "Error: Carpeta padre no existe. Use -r\n"; file.close(); return;
        }
        file.close();
        MkdirManager::Mkdir(parentPath, true);
        file = FileUtils::OpenFile(mounted->path);
        if(!file.is_open()){ std::cout << "Error: No se pudo reabrir el disco\n"; return; }
        // Re-leer partStart y sb porque MkdirManager puede haber cambiado bloques
        partStart = -1; partSize = -1;
        FileUtils::FindPartition(file, mounted->name, partStart, partSize);
        file.seekg(partStart);
        file.read(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));
    }

    int parentIdx = parentPath.empty() ? 0 : FindInodeByPath(file, sb, parentPath);
    if(parentIdx == -1){ std::cout << "Error: No se pudo encontrar carpeta padre\n"; file.close(); return; }

    Inode parentInode = BlockManager::ReadInode(file, sb, parentIdx);
    if(!HasWritePermission(parentInode)){ std::cout << "Error: Sin permiso de escritura\n"; file.close(); return; }

    // Verificar si ya existe
    int existingIdx = FindInodeByPath(file, sb, path);
    if(existingIdx != -1){
        Inode existing = BlockManager::ReadInode(file, sb, existingIdx);
        if(existing.i_type == 1){
            std::cout << "El archivo ya existe. ¿Desea sobreescribirlo? (s/n): ";
            std::string resp; std::getline(std::cin, resp);
            if(resp != "s" && resp != "S"){ std::cout << "Operación cancelada\n"; file.close(); return; }
            std::string content = "";
            if(!cont.empty()){
                std::ifstream cf(cont); if(!cf.is_open()){ std::cout << "Error: No se pudo abrir: " << cont << "\n"; file.close(); return; }
                std::stringstream buf; buf << cf.rdbuf(); content = buf.str();
            } else if(size > 0){ for(int i = 0; i < size; i++) content += (char)('0'+(i%10)); }
            if(!BlockManager::WriteFileContent(file, sb, partStart, existing, content)){ file.close(); return; }
            BlockManager::WriteInode(file, sb, existingIdx, existing);
            std::cout << "Archivo sobreescrito correctamente: " << path << "\n";
        } else {
            std::cout << "Error: Ya existe una carpeta con ese nombre\n";
        }
        file.close(); return;
    }

    // Determinar contenido
    std::string content = "";
    if(!cont.empty()){
        std::ifstream cf(cont); if(!cf.is_open()){ std::cout << "Error: No se pudo abrir: " << cont << "\n"; file.close(); return; }
        std::stringstream buf; buf << cf.rdbuf(); content = buf.str();
    } else if(size > 0){ for(int i = 0; i < size; i++) content += (char)('0'+(i%10)); }

    int newInodeIdx = BlockManager::AllocateInode(file, sb, partStart);
    if(newInodeIdx == -1){ file.close(); return; }

    Inode newInode{};
    newInode.i_uid = LoginManager::GetSessionUid();
    newInode.i_gid = LoginManager::GetSessionGid();
    newInode.i_size = 0;
    newInode.i_atime = newInode.i_ctime = newInode.i_mtime = time(nullptr);
    newInode.i_type = 1; newInode.i_perm = 664;
    for(int i = 0; i < 15; i++) newInode.i_block[i] = -1;

    if(!content.empty()){
        if(!BlockManager::WriteFileContent(file, sb, partStart, newInode, content)){
            BlockManager::FreeInode(file, sb, partStart, newInodeIdx); file.close(); return;
        }
    } else {
        newInode.i_size = 0;
        BlockManager::WriteInode(file, sb, newInodeIdx, newInode);
        BlockManager::UpdateSuperBlock(file, partStart, sb);
    }

    parentInode = BlockManager::ReadInode(file, sb, parentIdx);
    if(!BlockManager::AddEntryToFolder(file, sb, partStart, parentInode, parentIdx, fileName, newInodeIdx)){
        BlockManager::FreeInode(file, sb, partStart, newInodeIdx); file.close(); return;
    }
    BlockManager::WriteInode(file, sb, newInodeIdx, newInode);
    file.close();
    std::cout << "Archivo creado correctamente: " << path << "\n";
}

} // namespace MkfileManager