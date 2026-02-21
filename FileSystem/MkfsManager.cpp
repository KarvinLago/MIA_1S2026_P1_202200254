#include "MkfsManager.h"
#include "../Mount/MountManager.h"
#include "../Structs/Structs.h"
#include "../Utilities/FileUtils.h"
#include <iostream>
#include <cmath>
#include <cstring>
#include <ctime>

// Necesitamos acceder a la lista de montadas
extern std::vector<MountedPartition> mountedList;

namespace MkfsManager {

void Mkfs(const std::string& id)
{
    // ================= BUSCAR PARTICIÓN POR ID =================
    MountedPartition* mounted = nullptr;

    for(auto& m : mountedList){
        if(m.id == id){
            mounted = &m;
            break;
        }
    }

    if(mounted == nullptr){
        std::cout << "Error: ID no encontrado\n";
        return;
    }

    // ================= ABRIR DISCO =================
    auto file = FileUtils::OpenFile(mounted->path);
    if(!file.is_open()){
        std::cout << "Error: No se pudo abrir el disco\n";
        return;
    }

    // ================= LEER MBR =================
    MBR mbr{};
    file.seekg(0);
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

    // ================= LOCALIZAR PARTICIÓN =================
    Partition* partition = nullptr;

    for(int i = 0; i < 4; i++){
        if(mbr.mbr_partitions[i].part_s > 0){
            if(std::string(mbr.mbr_partitions[i].part_name) == mounted->name){
                partition = &mbr.mbr_partitions[i];
                break;
            }
        }
    }

    if(partition == nullptr){
        std::cout << "Error: Partición no encontrada\n";
        file.close();
        return;
    }

    int partitionSize = partition->part_s;

    // ================= CALCULAR n =================
    int n = (partitionSize - sizeof(SuperBlock)) /
            (4 + sizeof(Inode) + 3 * sizeof(FileBlock));

    if(n <= 0){
        std::cout << "Error: Tamaño insuficiente para formatear\n";
        file.close();
        return;
    }

    // ================= CONFIGURAR SUPER BLOQUE =================
    SuperBlock sb{};
    sb.s_filesystem_type = 2;
    sb.s_inodes_count = n;
    sb.s_blocks_count = 3 * n;
    sb.s_free_inodes_count = n - 1;  // usamos el 0
    sb.s_free_blocks_count = 3 * n - 1;
    sb.s_mtime = time(nullptr);
    sb.s_umtime = 0;
    sb.s_mnt_count = 1;
    sb.s_magic = 0xEF53;
    sb.s_inode_size = sizeof(Inode);
    sb.s_block_size = sizeof(FileBlock);
    sb.s_first_ino = 1;
    sb.s_first_blo = 1;

    int start = partition->part_start;

    sb.s_bm_inode_start = start + sizeof(SuperBlock);
    sb.s_bm_block_start = sb.s_bm_inode_start + n;
    sb.s_inode_start = sb.s_bm_block_start + (3 * n);
    sb.s_block_start = sb.s_inode_start + (n * sizeof(Inode));

    // ================= ESCRIBIR SUPER BLOQUE =================
    file.seekp(start);
    file.write(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));

    // ================= INICIALIZAR BITMAPS =================
    std::vector<char> bitmapInodes(n, '0');
    std::vector<char> bitmapBlocks(3*n, '0');

    bitmapInodes[0] = '1';
    bitmapBlocks[0] = '1';

    file.seekp(sb.s_bm_inode_start);
    file.write(bitmapInodes.data(), n);

    file.seekp(sb.s_bm_block_start);
    file.write(bitmapBlocks.data(), 3*n);

    // ================= CREAR INODO RAÍZ =================
    Inode root{};
    root.i_uid = 1;
    root.i_gid = 1;
    root.i_size = 0;
    root.i_atime = time(nullptr);
    root.i_ctime = time(nullptr);
    root.i_mtime = time(nullptr);
    root.i_type = 0;
    root.i_perm = 664;

    root.i_block[0] = 0;
    for(int i=1;i<15;i++)
        root.i_block[i] = -1;

    file.seekp(sb.s_inode_start);
    file.write(reinterpret_cast<char*>(&root), sizeof(Inode));

    // ================= CREAR BLOQUE RAÍZ =================
    FolderBlock folder{};
    strcpy(folder.b_content[0].b_name, ".");
    folder.b_content[0].b_inodo = 0;

    strcpy(folder.b_content[1].b_name, "..");
    folder.b_content[1].b_inodo = 0;

    strcpy(folder.b_content[2].b_name, "users.txt");
    folder.b_content[2].b_inodo = 1;

    folder.b_content[3].b_inodo = -1;

    file.seekp(sb.s_block_start);
    file.write(reinterpret_cast<char*>(&folder), sizeof(FolderBlock));

    file.close();

    std::cout << "Partición formateada correctamente (EXT2)\n";
}

}