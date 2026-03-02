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

namespace MkfileManager {

// ======================================================
// ================= SPLIT PATH =========================
// ======================================================

static std::vector<std::string> SplitPath(const std::string& path)
{
    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string token;
    while(std::getline(ss, token, '/'))
        if(!token.empty()) parts.push_back(token);
    return parts;
}

// ======================================================
// ================= FIND INODE BY PATH =================
// ======================================================

static int FindInodeByPath(std::fstream& file, SuperBlock& sb,
                           const std::string& path)
{
    std::vector<std::string> parts = SplitPath(path);
    if(parts.empty()) return 0;

    int blockSize = sizeof(FileBlock);
    int currentInodeIndex = 0;

    for(const std::string& part : parts)
    {
        Inode current = BlockManager::ReadInode(file, sb, currentInodeIndex);
        if(current.i_type != 0) return -1;

        bool found = false;
        for(int b = 0; b < 12 && !found; b++)
        {
            if(current.i_block[b] == -1) break;

            FolderBlock fb{};
            file.seekg(sb.s_block_start + (current.i_block[b] * blockSize));
            file.read(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));

            for(int e = 0; e < 4 && !found; e++)
            {
                if(fb.b_content[e].b_inodo == -1) continue;
                if(fb.b_content[e].b_name[0] == '\0') continue;
                std::string name(fb.b_content[e].b_name);
                if(name == "." || name == "..") continue;
                if(name == part)
                {
                    currentInodeIndex = fb.b_content[e].b_inodo;
                    found = true;
                }
            }
        }
        if(!found) return -1;
    }
    return currentInodeIndex;
}

// ======================================================
// ================= HAS WRITE PERMISSION ===============
// ======================================================

static bool HasWritePermission(Inode& inode)
{
    if(LoginManager::IsRoot()) return true;

    int perm = inode.i_perm;
    int uid  = LoginManager::GetSessionUid();
    int gid  = LoginManager::GetSessionGid();

    int permBit;
    if(inode.i_uid == uid)      permBit = (perm / 100) % 10;
    else if(inode.i_gid == gid) permBit = (perm / 10)  % 10;
    else                        permBit =  perm         % 10;

    return (permBit == 2 || permBit == 3 || permBit == 6 || permBit == 7);
}

// ======================================================
// ================= MKFILE =============================
// ======================================================

void Mkfile(const std::string& path,
            bool createParents,
            int size,
            const std::string& cont)
{
    if(!LoginManager::IsLogged())
    {
        std::cout << "Error: Debe iniciar sesión\n";
        return;
    }

    if(size < 0)
    {
        std::cout << "Error: El tamaño no puede ser negativo\n";
        return;
    }

    std::string id = LoginManager::GetSessionId();

    MountedPartition* mounted = nullptr;
    for(auto& m : mountedList)
        if(m.id == id) { mounted = &m; break; }

    if(!mounted)
    {
        std::cout << "Error: Partición no encontrada\n";
        return;
    }

    auto file = FileUtils::OpenFile(mounted->path);
    if(!file.is_open())
    {
        std::cout << "Error: No se pudo abrir el disco\n";
        return;
    }

    MBR mbr{};
    file.seekg(0);
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

    Partition* partition = nullptr;
    for(int i = 0; i < 4; i++)
    {
        if(mbr.mbr_partitions[i].part_s > 0 &&
           std::string(mbr.mbr_partitions[i].part_name) == mounted->name)
        {
            partition = &mbr.mbr_partitions[i];
            break;
        }
    }

    if(!partition)
    {
        std::cout << "Error: Partición no encontrada en disco\n";
        file.close();
        return;
    }

    SuperBlock sb{};
    file.seekg(partition->part_start);
    file.read(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));

    // ── Separar carpeta padre y nombre del archivo ──
    std::vector<std::string> parts = SplitPath(path);
    if(parts.empty())
    {
        std::cout << "Error: Ruta inválida\n";
        file.close();
        return;
    }

    std::string fileName = parts.back();
    std::string parentPath = "";
    for(int i = 0; i < (int)parts.size() - 1; i++)
        parentPath += "/" + parts[i];

    // ── Crear carpetas padre si -r y no existen ──
    if(!parentPath.empty())
    {
        int parentExists = FindInodeByPath(file, sb, parentPath);
        if(parentExists == -1)
        {
            if(!createParents)
            {
                std::cout << "Error: Carpeta padre no existe. Use -r para crearla\n";
                file.close();
                return;
            }
            // Cerrar y usar MkdirManager para crear carpetas padre
            file.close();
            MkdirManager::Mkdir(parentPath, true);

            // Reabrir
            file = FileUtils::OpenFile(mounted->path);
            if(!file.is_open())
            {
                std::cout << "Error: No se pudo reabrir el disco\n";
                return;
            }
            file.seekg(partition->part_start);
            file.read(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));
        }
    }

    // ── Buscar inodo de la carpeta padre ──
    int parentInodeIndex = parentPath.empty() ? 0
                           : FindInodeByPath(file, sb, parentPath);

    if(parentInodeIndex == -1)
    {
        std::cout << "Error: No se pudo encontrar la carpeta padre\n";
        file.close();
        return;
    }

    Inode parentInode = BlockManager::ReadInode(file, sb, parentInodeIndex);

    // ── Verificar permiso de escritura en carpeta padre ──
    if(!HasWritePermission(parentInode))
    {
        std::cout << "Error: Sin permiso de escritura en carpeta padre\n";
        file.close();
        return;
    }

    // ── Verificar si el archivo ya existe ──
    int existingInode = FindInodeByPath(file, sb, path);
    if(existingInode != -1)
    {
        Inode existing = BlockManager::ReadInode(file, sb, existingInode);
        if(existing.i_type == 1)
        {
            // Es archivo — preguntar si sobreescribir
            std::cout << "El archivo ya existe. ¿Desea sobreescribirlo? (s/n): ";
            std::string resp;
            std::getline(std::cin, resp);
            if(resp != "s" && resp != "S")
            {
                std::cout << "Operación cancelada\n";
                file.close();
                return;
            }

            // Sobreescribir: determinar contenido
            std::string content = "";
            if(!cont.empty())
            {
                std::ifstream contFile(cont);
                if(!contFile.is_open())
                {
                    std::cout << "Error: No se pudo abrir el archivo de contenido: "
                              << cont << "\n";
                    file.close();
                    return;
                }
                std::stringstream buf;
                buf << contFile.rdbuf();
                content = buf.str();
                contFile.close();
            }
            else if(size > 0)
            {
                for(int i = 0; i < size; i++)
                    content += (char)('0' + (i % 10));
            }

            if(!BlockManager::WriteFileContent(file, sb, partition->part_start,
                                               existing, content))
            {
                file.close();
                return;
            }

            BlockManager::WriteInode(file, sb, existingInode, existing);
            std::cout << "Archivo sobreescrito correctamente: " << path << "\n";
            file.close();
            return;
        }
        else
        {
            std::cout << "Error: Ya existe una carpeta con ese nombre\n";
            file.close();
            return;
        }
    }

    // ── Determinar contenido del archivo ──
    std::string content = "";

    if(!cont.empty())
    {
        // -cont tiene prioridad sobre -size
        std::ifstream contFile(cont);
        if(!contFile.is_open())
        {
            std::cout << "Error: No se pudo abrir el archivo de contenido: "
                      << cont << "\n";
            file.close();
            return;
        }
        std::stringstream buf;
        buf << contFile.rdbuf();
        content = buf.str();
        contFile.close();
    }
    else if(size > 0)
    {
        // Contenido: 012345678901234...
        for(int i = 0; i < size; i++)
            content += (char)('0' + (i % 10));
    }
    // Si size==0 y no hay cont: archivo vacío

    // ── Asignar inodo nuevo ──
    int newInodeIndex = BlockManager::AllocateInode(file, sb, partition->part_start);
    if(newInodeIndex == -1)
    {
        file.close();
        return;
    }

    // ── Crear inodo del archivo ──
    Inode newInode{};
    newInode.i_uid   = LoginManager::GetSessionUid();
    newInode.i_gid   = LoginManager::GetSessionGid();
    newInode.i_size  = 0;
    newInode.i_atime = time(nullptr);
    newInode.i_ctime = time(nullptr);
    newInode.i_mtime = time(nullptr);
    newInode.i_type  = 1; // archivo
    newInode.i_perm  = 664;
    for(int i = 0; i < 15; i++) newInode.i_block[i] = -1;

    // ── Escribir contenido si hay ──
    if(!content.empty())
    {
        if(!BlockManager::WriteFileContent(file, sb, partition->part_start,
                                           newInode, content))
        {
            BlockManager::FreeInode(file, sb, partition->part_start, newInodeIndex);
            file.close();
            return;
        }
    }
    else
    {
        newInode.i_size = 0;
        BlockManager::WriteInode(file, sb, newInodeIndex, newInode);
        BlockManager::UpdateSuperBlock(file, partition->part_start, sb);
    }

    // ── Agregar entrada en carpeta padre ──
    parentInode = BlockManager::ReadInode(file, sb, parentInodeIndex);
    if(!BlockManager::AddEntryToFolder(file, sb, partition->part_start,
                                       parentInode, parentInodeIndex,
                                       fileName, newInodeIndex))
    {
        BlockManager::FreeInode(file, sb, partition->part_start, newInodeIndex);
        file.close();
        return;
    }

    // ── Escribir inodo final al disco ──
    BlockManager::WriteInode(file, sb, newInodeIndex, newInode);

    file.close();
    std::cout << "Archivo creado correctamente: " << path << "\n";
}

} // namespace MkfileManager