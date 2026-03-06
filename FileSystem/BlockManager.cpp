#include "BlockManager.h"
#include <vector>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <ctime>

namespace BlockManager {

// ======================================================
// ================= SUPERBLOCK UPDATE ==================
// ======================================================

void UpdateSuperBlock(std::fstream& file, int partitionStart, SuperBlock& sb)
{
    file.seekp(partitionStart);
    file.write(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));
    file.flush();
}

// ======================================================
// ================= ALLOCATE BLOCK =====================
// ======================================================

int AllocateBlock(std::fstream& file, SuperBlock& sb, int partitionStart)
{
    if(sb.s_free_blocks_count <= 0)
    {
        std::cout << "Error: No hay bloques libres disponibles\n";
        return -1;
    }

    std::vector<char> bitmap(sb.s_blocks_count);
    file.seekg(sb.s_bm_block_start);
    file.read(bitmap.data(), sb.s_blocks_count);

    for(int i = 0; i < sb.s_blocks_count; i++)
    {
        if(bitmap[i] == '0')
        {
            bitmap[i] = '1';
            file.seekp(sb.s_bm_block_start);
            file.write(bitmap.data(), sb.s_blocks_count);
            sb.s_free_blocks_count--;
            sb.s_first_blo = sb.s_blocks_count;
            for(int j = i+1; j < sb.s_blocks_count; j++)
                if(bitmap[j] == '0') { sb.s_first_blo = j; break; }
            return i;
        }
    }
    return -1;
}

// ======================================================
// ================= FREE BLOCK =========================
// ======================================================

void FreeBlock(std::fstream& file, SuperBlock& sb, int partitionStart, int blockIndex)
{
    if(blockIndex < 0 || blockIndex >= sb.s_blocks_count) return;

    std::vector<char> bitmap(sb.s_blocks_count);
    file.seekg(sb.s_bm_block_start);
    file.read(bitmap.data(), sb.s_blocks_count);

    if(bitmap[blockIndex] == '1')
    {
        bitmap[blockIndex] = '0';
        file.seekp(sb.s_bm_block_start);
        file.write(bitmap.data(), sb.s_blocks_count);
        sb.s_free_blocks_count++;
        if(blockIndex < sb.s_first_blo) sb.s_first_blo = blockIndex;
    }
}

// ======================================================
// ================= ALLOCATE INODE =====================
// ======================================================

int AllocateInode(std::fstream& file, SuperBlock& sb, int partitionStart)
{
    if(sb.s_free_inodes_count <= 0)
    {
        std::cout << "Error: No hay inodos libres disponibles\n";
        return -1;
    }

    std::vector<char> bitmap(sb.s_inodes_count);
    file.seekg(sb.s_bm_inode_start);
    file.read(bitmap.data(), sb.s_inodes_count);

    for(int i = 0; i < sb.s_inodes_count; i++)
    {
        if(bitmap[i] == '0')
        {
            bitmap[i] = '1';
            file.seekp(sb.s_bm_inode_start);
            file.write(bitmap.data(), sb.s_inodes_count);
            sb.s_free_inodes_count--;
            sb.s_first_ino = sb.s_inodes_count;
            for(int j = i+1; j < sb.s_inodes_count; j++)
                if(bitmap[j] == '0') { sb.s_first_ino = j; break; }
            return i;
        }
    }
    return -1;
}

// ======================================================
// ================= FREE INODE =========================
// ======================================================

void FreeInode(std::fstream& file, SuperBlock& sb, int partitionStart, int inodeIndex)
{
    if(inodeIndex < 0 || inodeIndex >= sb.s_inodes_count) return;

    std::vector<char> bitmap(sb.s_inodes_count);
    file.seekg(sb.s_bm_inode_start);
    file.read(bitmap.data(), sb.s_inodes_count);

    if(bitmap[inodeIndex] == '1')
    {
        bitmap[inodeIndex] = '0';
        file.seekp(sb.s_bm_inode_start);
        file.write(bitmap.data(), sb.s_inodes_count);
        sb.s_free_inodes_count++;
        if(inodeIndex < sb.s_first_ino) sb.s_first_ino = inodeIndex;
    }
}

// ======================================================
// ================= READ INODE =========================
// ======================================================

Inode ReadInode(std::fstream& file, SuperBlock& sb, int inodeIndex)
{
    Inode inode{};
    file.seekg(sb.s_inode_start + (inodeIndex * sizeof(Inode)));
    file.read(reinterpret_cast<char*>(&inode), sizeof(Inode));
    return inode;
}

// ======================================================
// ================= WRITE INODE ========================
// ======================================================

void WriteInode(std::fstream& file, SuperBlock& sb, int inodeIndex, Inode& inode)
{
    file.seekp(sb.s_inode_start + (inodeIndex * sizeof(Inode)));
    file.write(reinterpret_cast<char*>(&inode), sizeof(Inode));
}

// ======================================================
// ================= READ FILE ==========================
// ======================================================

std::string ReadFileContent(std::fstream& file, SuperBlock& sb, Inode& inode)
{
    std::string content;
    if(inode.i_size <= 0) return content;

    int bytesRemaining = inode.i_size;
    int blockSize      = sizeof(FileBlock);

    auto readDataBlock = [&](int blockIndex)
    {
        if(bytesRemaining <= 0) return;
        if(blockIndex < 0 || blockIndex >= sb.s_blocks_count) return;
        FileBlock block{};
        file.seekg(sb.s_block_start + (blockIndex * blockSize));
        file.read(reinterpret_cast<char*>(&block), blockSize);
        int toCopy = std::min(bytesRemaining, blockSize);
        content.append(block.b_content, toCopy);
        bytesRemaining -= toCopy;
    };

    for(int i = 0; i < 12 && bytesRemaining > 0; i++)
    {
        if(inode.i_block[i] == -1) break;
        readDataBlock(inode.i_block[i]);
    }

    if(bytesRemaining > 0 && inode.i_block[12] != -1)
    {
        PointerBlock pb{};
        file.seekg(sb.s_block_start + (inode.i_block[12] * blockSize));
        file.read(reinterpret_cast<char*>(&pb), sizeof(PointerBlock));
        for(int i = 0; i < 16 && bytesRemaining > 0; i++)
        {
            if(pb.b_pointers[i] == -1) break;
            readDataBlock(pb.b_pointers[i]);
        }
    }

    if(bytesRemaining > 0 && inode.i_block[13] != -1)
    {
        PointerBlock level1{};
        file.seekg(sb.s_block_start + (inode.i_block[13] * blockSize));
        file.read(reinterpret_cast<char*>(&level1), sizeof(PointerBlock));
        for(int i = 0; i < 16 && bytesRemaining > 0; i++)
        {
            if(level1.b_pointers[i] == -1) break;
            PointerBlock level2{};
            file.seekg(sb.s_block_start + (level1.b_pointers[i] * blockSize));
            file.read(reinterpret_cast<char*>(&level2), sizeof(PointerBlock));
            for(int j = 0; j < 16 && bytesRemaining > 0; j++)
            {
                if(level2.b_pointers[j] == -1) break;
                readDataBlock(level2.b_pointers[j]);
            }
        }
    }

    if(bytesRemaining > 0 && inode.i_block[14] != -1)
    {
        PointerBlock level1{};
        file.seekg(sb.s_block_start + (inode.i_block[14] * blockSize));
        file.read(reinterpret_cast<char*>(&level1), sizeof(PointerBlock));
        for(int i = 0; i < 16 && bytesRemaining > 0; i++)
        {
            if(level1.b_pointers[i] == -1) break;
            PointerBlock level2{};
            file.seekg(sb.s_block_start + (level1.b_pointers[i] * blockSize));
            file.read(reinterpret_cast<char*>(&level2), sizeof(PointerBlock));
            for(int j = 0; j < 16 && bytesRemaining > 0; j++)
            {
                if(level2.b_pointers[j] == -1) break;
                PointerBlock level3{};
                file.seekg(sb.s_block_start + (level2.b_pointers[j] * blockSize));
                file.read(reinterpret_cast<char*>(&level3), sizeof(PointerBlock));
                for(int k = 0; k < 16 && bytesRemaining > 0; k++)
                {
                    if(level3.b_pointers[k] == -1) break;
                    readDataBlock(level3.b_pointers[k]);
                }
            }
        }
    }

    return content;
}

// ======================================================
// ================= WRITE FILE =========================
// ======================================================

bool WriteFileContent(std::fstream& file, SuperBlock& sb, int partitionStart,
                      Inode& inode, const std::string& content)
{
    int blockSize = sizeof(FileBlock);

    // Liberar directos
    for(int i = 0; i < 12; i++)
        if(inode.i_block[i] != -1) { FreeBlock(file,sb,partitionStart,inode.i_block[i]); inode.i_block[i]=-1; }

    // Liberar simple
    if(inode.i_block[12] != -1)
    {
        PointerBlock pb{};
        file.seekg(sb.s_block_start+(inode.i_block[12]*blockSize));
        file.read(reinterpret_cast<char*>(&pb),sizeof(PointerBlock));
        for(int i=0;i<16;i++) if(pb.b_pointers[i]!=-1) FreeBlock(file,sb,partitionStart,pb.b_pointers[i]);
        FreeBlock(file,sb,partitionStart,inode.i_block[12]); inode.i_block[12]=-1;
    }

    // Liberar doble
    if(inode.i_block[13] != -1)
    {
        PointerBlock l1{};
        file.seekg(sb.s_block_start+(inode.i_block[13]*blockSize));
        file.read(reinterpret_cast<char*>(&l1),sizeof(PointerBlock));
        for(int i=0;i<16;i++){
            if(l1.b_pointers[i]==-1) continue;
            PointerBlock l2{};
            file.seekg(sb.s_block_start+(l1.b_pointers[i]*blockSize));
            file.read(reinterpret_cast<char*>(&l2),sizeof(PointerBlock));
            for(int j=0;j<16;j++) if(l2.b_pointers[j]!=-1) FreeBlock(file,sb,partitionStart,l2.b_pointers[j]);
            FreeBlock(file,sb,partitionStart,l1.b_pointers[i]);
        }
        FreeBlock(file,sb,partitionStart,inode.i_block[13]); inode.i_block[13]=-1;
    }

    // Liberar triple
    if(inode.i_block[14] != -1)
    {
        PointerBlock l1{};
        file.seekg(sb.s_block_start+(inode.i_block[14]*blockSize));
        file.read(reinterpret_cast<char*>(&l1),sizeof(PointerBlock));
        for(int i=0;i<16;i++){
            if(l1.b_pointers[i]==-1) continue;
            PointerBlock l2{};
            file.seekg(sb.s_block_start+(l1.b_pointers[i]*blockSize));
            file.read(reinterpret_cast<char*>(&l2),sizeof(PointerBlock));
            for(int j=0;j<16;j++){
                if(l2.b_pointers[j]==-1) continue;
                PointerBlock l3{};
                file.seekg(sb.s_block_start+(l2.b_pointers[j]*blockSize));
                file.read(reinterpret_cast<char*>(&l3),sizeof(PointerBlock));
                for(int k=0;k<16;k++) if(l3.b_pointers[k]!=-1) FreeBlock(file,sb,partitionStart,l3.b_pointers[k]);
                FreeBlock(file,sb,partitionStart,l2.b_pointers[j]);
            }
            FreeBlock(file,sb,partitionStart,l1.b_pointers[i]);
        }
        FreeBlock(file,sb,partitionStart,inode.i_block[14]); inode.i_block[14]=-1;
    }

    int contentSize  = (int)content.size();
    int blocksNeeded = (contentSize + blockSize - 1) / blockSize;
    int maxBlocks    = 12 + 16 + (16*16) + (16*16*16);

    if(blocksNeeded > maxBlocks)  { std::cout << "Error: Contenido excede capacidad máxima\n"; return false; }
    if(blocksNeeded > sb.s_free_blocks_count) { std::cout << "Error: No hay suficientes bloques libres\n"; return false; }

    int remaining=blocksNeeded, contentOffset=0;

    auto writeDataBlock = [&](int blockIndex) -> bool {
        FileBlock block{};
        memset(block.b_content,0,blockSize);
        int s=std::min(blockSize,contentSize-contentOffset);
        if(s>0) memcpy(block.b_content,content.c_str()+contentOffset,s);
        file.seekp(sb.s_block_start+(blockIndex*blockSize));
        file.write(reinterpret_cast<char*>(&block),blockSize);
        contentOffset+=s; remaining--; return true;
    };

    for(int i=0;i<12&&remaining>0;i++){
        int nb=AllocateBlock(file,sb,partitionStart); if(nb==-1) return false;
        inode.i_block[i]=nb; writeDataBlock(nb);
    }
    if(remaining>0){
        int pi=AllocateBlock(file,sb,partitionStart); if(pi==-1) return false;
        inode.i_block[12]=pi;
        PointerBlock pb{}; for(int i=0;i<16;i++) pb.b_pointers[i]=-1;
        for(int i=0;i<16&&remaining>0;i++){
            int nb=AllocateBlock(file,sb,partitionStart); if(nb==-1) return false;
            pb.b_pointers[i]=nb; writeDataBlock(nb);
        }
        file.seekp(sb.s_block_start+(pi*blockSize));
        file.write(reinterpret_cast<char*>(&pb),sizeof(PointerBlock));
    }
    if(remaining>0){
        int l1=AllocateBlock(file,sb,partitionStart); if(l1==-1) return false;
        inode.i_block[13]=l1;
        PointerBlock lv1{}; for(int i=0;i<16;i++) lv1.b_pointers[i]=-1;
        for(int i=0;i<16&&remaining>0;i++){
            int l2=AllocateBlock(file,sb,partitionStart); if(l2==-1) return false;
            lv1.b_pointers[i]=l2;
            PointerBlock lv2{}; for(int j=0;j<16;j++) lv2.b_pointers[j]=-1;
            for(int j=0;j<16&&remaining>0;j++){
                int nb=AllocateBlock(file,sb,partitionStart); if(nb==-1) return false;
                lv2.b_pointers[j]=nb; writeDataBlock(nb);
            }
            file.seekp(sb.s_block_start+(l2*blockSize));
            file.write(reinterpret_cast<char*>(&lv2),sizeof(PointerBlock));
        }
        file.seekp(sb.s_block_start+(l1*blockSize));
        file.write(reinterpret_cast<char*>(&lv1),sizeof(PointerBlock));
    }
    if(remaining>0){
        int l1=AllocateBlock(file,sb,partitionStart); if(l1==-1) return false;
        inode.i_block[14]=l1;
        PointerBlock lv1{}; for(int i=0;i<16;i++) lv1.b_pointers[i]=-1;
        for(int i=0;i<16&&remaining>0;i++){
            int l2=AllocateBlock(file,sb,partitionStart); if(l2==-1) return false;
            lv1.b_pointers[i]=l2;
            PointerBlock lv2{}; for(int j=0;j<16;j++) lv2.b_pointers[j]=-1;
            for(int j=0;j<16&&remaining>0;j++){
                int l3=AllocateBlock(file,sb,partitionStart); if(l3==-1) return false;
                lv2.b_pointers[j]=l3;
                PointerBlock lv3{}; for(int k=0;k<16;k++) lv3.b_pointers[k]=-1;
                for(int k=0;k<16&&remaining>0;k++){
                    int nb=AllocateBlock(file,sb,partitionStart); if(nb==-1) return false;
                    lv3.b_pointers[k]=nb; writeDataBlock(nb);
                }
                file.seekp(sb.s_block_start+(l3*blockSize));
                file.write(reinterpret_cast<char*>(&lv3),sizeof(PointerBlock));
            }
            file.seekp(sb.s_block_start+(l2*blockSize));
            file.write(reinterpret_cast<char*>(&lv2),sizeof(PointerBlock));
        }
        file.seekp(sb.s_block_start+(l1*blockSize));
        file.write(reinterpret_cast<char*>(&lv1),sizeof(PointerBlock));
    }

    inode.i_size  = contentSize;
    inode.i_mtime = time(nullptr);
    UpdateSuperBlock(file, partitionStart, sb);
    return true;
}

// ======================================================
// ================= ADD ENTRY TO FOLDER ================
// ======================================================

bool AddEntryToFolder(std::fstream& file, SuperBlock& sb, int partitionStart,
                      Inode& folderInode, int folderInodeIndex,
                      const std::string& entryName, int entryInodeIndex)
{
    int blockSize = sizeof(FileBlock);

    // Buscar espacio libre en bloques directos existentes
    for(int b = 0; b < 12; b++)
    {
        if(folderInode.i_block[b] == -1) break;

        FolderBlock fb{};
        int blockPos = sb.s_block_start + (folderInode.i_block[b] * blockSize);
        file.seekg(blockPos);
        file.read(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));

        for(int e = 0; e < 4; e++)
        {
            if(fb.b_content[e].b_inodo == -1 ||
               fb.b_content[e].b_name[0] == '\0')
            {
                memset(fb.b_content[e].b_name, 0, 12);
                strncpy(fb.b_content[e].b_name, entryName.c_str(), 11);
                fb.b_content[e].b_inodo = entryInodeIndex;

                file.seekp(blockPos);
                file.write(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));

                folderInode.i_mtime = time(nullptr);
                WriteInode(file, sb, folderInodeIndex, folderInode);
                UpdateSuperBlock(file, partitionStart, sb);
                return true;
            }
        }
    }

    // Asignar nuevo bloque carpeta
    for(int b = 0; b < 12; b++)
    {
        if(folderInode.i_block[b] == -1)
        {
            int newBlock = AllocateBlock(file, sb, partitionStart);
            if(newBlock == -1) return false;

            folderInode.i_block[b] = newBlock;

            FolderBlock fb{};
            for(int e = 0; e < 4; e++) fb.b_content[e].b_inodo = -1;

            memset(fb.b_content[0].b_name, 0, 12);
            strncpy(fb.b_content[0].b_name, entryName.c_str(), 11);
            fb.b_content[0].b_inodo = entryInodeIndex;

            int blockPos = sb.s_block_start + (newBlock * blockSize);
            file.seekp(blockPos);
            file.write(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));

            folderInode.i_mtime = time(nullptr);
            WriteInode(file, sb, folderInodeIndex, folderInode);
            UpdateSuperBlock(file, partitionStart, sb);
            return true;
        }
    }

    std::cout << "Error: Carpeta llena\n";
    return false;
}

} // namespace BlockManager
