#include "MkfsManager.h"
#include "../Mount/MountManager.h"
#include "../Structs/Structs.h"
#include "../Utilities/FileUtils.h"
#include <iostream>
#include <cmath>
#include <cstring>
#include <ctime>

extern std::vector<MountedPartition> mountedList;

namespace MkfsManager {

void Mkfs(const std::string& id)
{
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

    auto file = FileUtils::OpenFile(mounted->path);
    if(!file.is_open()){
        std::cout << "Error: No se pudo abrir el disco\n";
        return;
    }

    // ================= LOCALIZAR PARTICIÓN =================

    MBR mbr{};
    file.seekg(0);
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

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
    int start = partition->part_start;

    int n = (partitionSize - sizeof(SuperBlock)) /
            (4 + sizeof(Inode) + 3 * sizeof(FileBlock));

    if(n <= 0){
        std::cout << "Error: Tamaño insuficiente\n";
        file.close();
        return;
    }

    // ================= SUPER BLOQUE =================

    SuperBlock sb{};
    sb.s_filesystem_type = 2;
    sb.s_inodes_count = n;
    sb.s_blocks_count = 3*n;
    sb.s_free_inodes_count = n - 2;
    sb.s_free_blocks_count = 3*n - 2;
    sb.s_mtime = time(nullptr);
    sb.s_umtime = 0;
    sb.s_mnt_count = 1;
    sb.s_magic = 0xEF53;
    sb.s_inode_size = sizeof(Inode);
    sb.s_block_size = sizeof(FileBlock);
    sb.s_first_ino = 2;
    sb.s_first_blo = 2;

    sb.s_bm_inode_start = start + sizeof(SuperBlock);
    sb.s_bm_block_start = sb.s_bm_inode_start + n;
    sb.s_inode_start = sb.s_bm_block_start + (3*n);
    sb.s_block_start = sb.s_inode_start + (n * sizeof(Inode));

    file.seekp(start);
    file.write(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));

    // ================= BITMAPS =================

    std::vector<char> bitmapInodes(n, '0');
    std::vector<char> bitmapBlocks(3*n, '0');

    bitmapInodes[0] = '1'; // raíz
    bitmapInodes[1] = '1'; // users.txt

    bitmapBlocks[0] = '1'; // carpeta raíz
    bitmapBlocks[1] = '1'; // contenido users.txt

    file.seekp(sb.s_bm_inode_start);
    file.write(bitmapInodes.data(), n);

    file.seekp(sb.s_bm_block_start);
    file.write(bitmapBlocks.data(), 3*n);

    // ================= INODO RAÍZ =================

    Inode root{};
    root.i_uid = 1;
    root.i_gid = 1;
    root.i_size = 0;
    root.i_atime = time(nullptr);
    root.i_ctime = time(nullptr);
    root.i_mtime = time(nullptr);
    root.i_type = 0;
    root.i_perm = 664;

    for(int i=0;i<15;i++) root.i_block[i] = -1;
    root.i_block[0] = 0;

    file.seekp(sb.s_inode_start);
    file.write(reinterpret_cast<char*>(&root), sizeof(Inode));

    // ================= INODO users.txt =================

    std::string initialContent = "1,G,root\n1,U,root,root,123\n";

    Inode users{};
    users.i_uid = 1;
    users.i_gid = 1;
    users.i_size = initialContent.size();
    users.i_atime = time(nullptr);
    users.i_ctime = time(nullptr);
    users.i_mtime = time(nullptr);
    users.i_type = 1;
    users.i_perm = 664;

    for(int i=0;i<15;i++) users.i_block[i] = -1;
    users.i_block[0] = 1;

    file.write(reinterpret_cast<char*>(&users), sizeof(Inode));

    // ================= BLOQUE RAÍZ =================

    FolderBlock folder{};
    strcpy(folder.b_content[0].b_name, ".");
    folder.b_content[0].b_inodo = 0;

    strcpy(folder.b_content[1].b_name, "..");
    folder.b_content[1].b_inodo = 0;

    strcpy(folder.b_content[2].b_name, "users.txt");
    folder.b_content[2].b_inodo = 1;

    folder.b_content[3].b_inodo = -1;

    int rootBlockPos = sb.s_block_start + (0 * sizeof(FileBlock));
    file.seekp(rootBlockPos);
    file.write(reinterpret_cast<char*>(&folder), sizeof(FolderBlock));

    // ================= BLOQUE users.txt =================

    FileBlock fileBlock{};
    memset(fileBlock.b_content, 0, sizeof(fileBlock.b_content));
    memcpy(fileBlock.b_content, initialContent.c_str(), initialContent.size());

    int usersBlockPos = sb.s_block_start + (1 * sizeof(FileBlock));
    file.seekp(usersBlockPos);
    file.write(reinterpret_cast<char*>(&fileBlock), sizeof(FileBlock));

    file.close();

    std::cout << "Partición formateada correctamente con users.txt (EXT2)\n";
}

}