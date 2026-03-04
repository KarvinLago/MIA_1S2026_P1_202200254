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
        if(m.id == id){ mounted = &m; break; }
    }

    if(!mounted){
        std::cout << "Error: ID no encontrado\n";
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

    // ── Buscar partición: primero en MBR, luego en EBR ──
    int partitionStart = -1;
    int partitionSize  = -1;

    // Buscar en MBR
    for(int i = 0; i < 4; i++){
        if(mbr.mbr_partitions[i].part_s > 0 &&
           std::string(mbr.mbr_partitions[i].part_name) == mounted->name){
            partitionStart = mbr.mbr_partitions[i].part_start;
            partitionSize  = mbr.mbr_partitions[i].part_s;
            break;
        }
    }

    // Si no encontró, buscar en EBR
    if(partitionStart == -1)
    {
        for(int i = 0; i < 4; i++)
        {
            if(mbr.mbr_partitions[i].part_s > 0 &&
               (mbr.mbr_partitions[i].part_type == 'e' ||
                mbr.mbr_partitions[i].part_type == 'E'))
            {
                int ebrPos = mbr.mbr_partitions[i].part_start;
                while(ebrPos != -1)
                {
                    EBR ebr{};
                    file.seekg(ebrPos);
                    file.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));

                    if(ebr.part_s <= 0) break;

                    if(std::string(ebr.part_name) == mounted->name){
                        partitionStart = ebr.part_start;
                        partitionSize  = ebr.part_s;
                        break;
                    }

                    ebrPos = ebr.part_next;
                }
                break;
            }
        }
    }

    if(partitionStart == -1){
        std::cout << "Error: Partición no encontrada\n";
        file.close();
        return;
    }

    // ================= CALCULAR n =================

    int n = (partitionSize - sizeof(SuperBlock)) /
            (4 + sizeof(Inode) + 3 * sizeof(FileBlock));

    if(n <= 0){
        std::cout << "Error: Tamaño insuficiente\n";
        file.close();
        return;
    }

    // ================= SUPER BLOQUE =================

    SuperBlock sb{};
    sb.s_filesystem_type    = 2;
    sb.s_inodes_count       = n;
    sb.s_blocks_count       = 3*n;
    sb.s_free_inodes_count  = n - 2;
    sb.s_free_blocks_count  = 3*n - 2;
    sb.s_mtime              = time(nullptr);
    sb.s_umtime             = 0;
    sb.s_mnt_count          = 1;
    sb.s_magic              = 0xEF53;
    sb.s_inode_size         = sizeof(Inode);
    sb.s_block_size         = sizeof(FileBlock);
    sb.s_first_ino          = 2;
    sb.s_first_blo          = 2;

    sb.s_bm_inode_start = partitionStart + sizeof(SuperBlock);
    sb.s_bm_block_start = sb.s_bm_inode_start + n;
    sb.s_inode_start    = sb.s_bm_block_start + (3*n);
    sb.s_block_start    = sb.s_inode_start + (n * sizeof(Inode));

    file.seekp(partitionStart);
    file.write(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));

    // ================= BITMAPS =================

    std::vector<char> bitmapInodes(n, '0');
    std::vector<char> bitmapBlocks(3*n, '0');

    bitmapInodes[0] = '1';
    bitmapInodes[1] = '1';
    bitmapBlocks[0] = '1';
    bitmapBlocks[1] = '1';

    file.seekp(sb.s_bm_inode_start);
    file.write(bitmapInodes.data(), n);

    file.seekp(sb.s_bm_block_start);
    file.write(bitmapBlocks.data(), 3*n);

    // ================= INODO RAÍZ =================

    Inode root{};
    root.i_uid = 1; root.i_gid = 1; root.i_size = 0;
    root.i_atime = root.i_ctime = root.i_mtime = time(nullptr);
    root.i_type = 0; root.i_perm = 664;
    for(int i=0;i<15;i++) root.i_block[i] = -1;
    root.i_block[0] = 0;

    file.seekp(sb.s_inode_start);
    file.write(reinterpret_cast<char*>(&root), sizeof(Inode));

    // ================= INODO users.txt =================

    std::string initialContent = "1,G,root\n1,U,root,root,123\n";

    Inode users{};
    users.i_uid = 1; users.i_gid = 1;
    users.i_size = initialContent.size();
    users.i_atime = users.i_ctime = users.i_mtime = time(nullptr);
    users.i_type = 1; users.i_perm = 664;
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

    file.seekp(sb.s_block_start);
    file.write(reinterpret_cast<char*>(&folder), sizeof(FolderBlock));

    // ================= BLOQUE users.txt =================

    FileBlock fileBlock{};
    memset(fileBlock.b_content, 0, sizeof(fileBlock.b_content));
    memcpy(fileBlock.b_content, initialContent.c_str(), initialContent.size());

    file.seekp(sb.s_block_start + sizeof(FileBlock));
    file.write(reinterpret_cast<char*>(&fileBlock), sizeof(FileBlock));

    file.close();
    std::cout << "Partición formateada correctamente con users.txt (EXT2)\n";
}

} // namespace MkfsManager