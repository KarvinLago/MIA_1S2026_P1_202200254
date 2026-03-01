// #include "BlockManager.h"
// #include <vector>
// #include <cstring>
// #include <cmath>
// #include <iostream>

// namespace BlockManager {

// // ======================================================
// // ================= SUPERBLOCK UPDATE ==================
// // ======================================================

// void UpdateSuperBlock(std::fstream& file, int partitionStart, SuperBlock& sb)
// {
//     file.seekp(partitionStart);
//     file.write(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));
// }

// // ======================================================
// // ================= ALLOCATE BLOCK =====================
// // ======================================================

// int AllocateBlock(std::fstream& file, SuperBlock& sb, int partitionStart)
// {
//     std::vector<char> bitmap(sb.s_blocks_count);

//     file.seekg(sb.s_bm_block_start);
//     file.read(bitmap.data(), sb.s_blocks_count);

//     for(int i = 0; i < sb.s_blocks_count; i++)
//     {
//         if(bitmap[i] == '0')
//         {
//             bitmap[i] = '1';

//             file.seekp(sb.s_bm_block_start);
//             file.write(bitmap.data(), sb.s_blocks_count);

//             sb.s_free_blocks_count--;

//             for(int j = 0; j < sb.s_blocks_count; j++)
//             {
//                 if(bitmap[j] == '0')
//                 {
//                     sb.s_first_blo = j;
//                     break;
//                 }
//             }

//             UpdateSuperBlock(file, partitionStart, sb);

//             return i;
//         }
//     }

//     return -1;
// }

// // ======================================================
// // ================= FREE BLOCK =========================
// // ======================================================

// void FreeBlock(std::fstream& file, SuperBlock& sb, int partitionStart, int blockIndex)
// {
//     if(blockIndex < 0) return;

//     std::vector<char> bitmap(sb.s_blocks_count);

//     file.seekg(sb.s_bm_block_start);
//     file.read(bitmap.data(), sb.s_blocks_count);

//     if(bitmap[blockIndex] == '1')
//     {
//         bitmap[blockIndex] = '0';

//         file.seekp(sb.s_bm_block_start);
//         file.write(bitmap.data(), sb.s_blocks_count);

//         sb.s_free_blocks_count++;

//         if(blockIndex < sb.s_first_blo)
//             sb.s_first_blo = blockIndex;

//         UpdateSuperBlock(file, partitionStart, sb);
//     }
// }

// // ======================================================
// // ================= READ FILE ==========================
// // ======================================================

// std::string ReadFileContent(std::fstream& file,
//                             SuperBlock& sb,
//                             Inode& inode)
// {
//     std::string content;

//     // int blocksNeeded =
//     //     std::ceil((double)inode.i_size / sizeof(FileBlock));

//     int blocksNeeded =
//     (inode.i_size + sizeof(FileBlock) - 1) / sizeof(FileBlock);

//     int blocksRead = 0;

//     // ================= DIRECTOS =================
//     for(int i = 0; i < 12 && blocksRead < blocksNeeded; i++)
//     {
//         if(inode.i_block[i] == -1) break;

//         FileBlock block{};
//         int blockPos = sb.s_block_start +
//                        (inode.i_block[i] * sizeof(FileBlock));

//         file.seekg(blockPos);
//         file.read(reinterpret_cast<char*>(&block),
//                   sizeof(FileBlock));

//         content.append(block.b_content,
//                        sizeof(FileBlock));

//         blocksRead++;
//     }

//     // ================= INDIRECTO SIMPLE =================
//     if(blocksRead < blocksNeeded && inode.i_block[12] != -1)
//     {
//         PointerBlock pb{};
//         int pointerPos = sb.s_block_start +
//                          (inode.i_block[12] * sizeof(FileBlock));

//         file.seekg(pointerPos);
//         file.read(reinterpret_cast<char*>(&pb),
//                   sizeof(PointerBlock));

//         for(int i = 0; i < 16 && blocksRead < blocksNeeded; i++)
//         {
//             if(pb.b_pointers[i] == -1) break;

//             FileBlock block{};
//             int blockPos = sb.s_block_start +
//                            (pb.b_pointers[i] * sizeof(FileBlock));

//             file.seekg(blockPos);
//             file.read(reinterpret_cast<char*>(&block),
//                       sizeof(FileBlock));

//             content.append(block.b_content,
//                            sizeof(FileBlock));

//             blocksRead++;
//         }
//     }

//     content.resize(inode.i_size);
//     return content;
// }

// // ======================================================
// // ================= WRITE FILE =========================
// // ======================================================

// bool WriteFileContent(std::fstream& file,
//                       SuperBlock& sb,
//                       int partitionStart,
//                       Inode& inode,
//                       const std::string& content)
// {
//     // int blocksNeeded =
//     //     std::ceil((double)content.size() / sizeof(FileBlock));

//     int blocksNeeded =
//     (content.size() + sizeof(FileBlock) - 1) / sizeof(FileBlock);

//     if(blocksNeeded > 28) // 12 directos + 16 indirecto simple
//     {
//         std::cout << "Error: Archivo excede capacidad Fase 2 (directos + simple)\n";
//         return false;
//     }

//     // ================= LIBERAR BLOQUES ANTERIORES =================

//     for(int i = 0; i < 12; i++)
//     {
//         if(inode.i_block[i] != -1)
//         {
//             FreeBlock(file, sb, partitionStart, inode.i_block[i]);
//             inode.i_block[i] = -1;
//         }
//     }

//     if(inode.i_block[12] != -1)
//     {
//         PointerBlock pb{};
//         int pointerPos = sb.s_block_start +
//                          (inode.i_block[12] * sizeof(FileBlock));

//         file.seekg(pointerPos);
//         file.read(reinterpret_cast<char*>(&pb),
//                   sizeof(PointerBlock));

//         for(int i = 0; i < 16; i++)
//         {
//             if(pb.b_pointers[i] != -1)
//                 FreeBlock(file, sb, partitionStart, pb.b_pointers[i]);
//         }

//         FreeBlock(file, sb, partitionStart, inode.i_block[12]);
//         inode.i_block[12] = -1;
//     }

//     // ================= ASIGNAR BLOQUES NUEVOS =================

//     int contentOffset = 0;

//     // ---------- DIRECTOS ----------
//     for(int i = 0; i < blocksNeeded && i < 12; i++)
//     {
//         int newBlock = AllocateBlock(file, sb, partitionStart);
//         if(newBlock == -1) return false;

//         inode.i_block[i] = newBlock;

//         FileBlock block{};
//         memset(block.b_content, 0, sizeof(block.b_content));

//         int sizeToCopy =
//             std::min((int)sizeof(FileBlock),
//                      (int)content.size() - contentOffset);

//         memcpy(block.b_content,
//                content.c_str() + contentOffset,
//                sizeToCopy);

//         int blockPos = sb.s_block_start +
//                        (newBlock * sizeof(FileBlock));

//         file.seekp(blockPos);
//         file.write(reinterpret_cast<char*>(&block),
//                    sizeof(FileBlock));

//         contentOffset += sizeToCopy;
//     }

//     // ---------- INDIRECTO SIMPLE ----------
//     if(blocksNeeded > 12)
//     {
//         int pointerBlockIndex = AllocateBlock(file, sb, partitionStart);
//         if(pointerBlockIndex == -1) return false;

//         inode.i_block[12] = pointerBlockIndex;

//         PointerBlock pb{};
//         for(int i = 0; i < 16; i++)
//             pb.b_pointers[i] = -1;

//         int remainingBlocks = blocksNeeded - 12;

//         for(int i = 0; i < remainingBlocks; i++)
//         {
//             int newBlock = AllocateBlock(file, sb, partitionStart);
//             if(newBlock == -1) return false;

//             pb.b_pointers[i] = newBlock;

//             FileBlock block{};
//             memset(block.b_content, 0, sizeof(block.b_content));

//             int sizeToCopy =
//                 std::min((int)sizeof(FileBlock),
//                          (int)content.size() - contentOffset);

//             memcpy(block.b_content,
//                    content.c_str() + contentOffset,
//                    sizeToCopy);

//             int blockPos = sb.s_block_start +
//                            (newBlock * sizeof(FileBlock));

//             file.seekp(blockPos);
//             file.write(reinterpret_cast<char*>(&block),
//                        sizeof(FileBlock));

//             contentOffset += sizeToCopy;
//         }

//         int pointerPos = sb.s_block_start +
//                          (pointerBlockIndex * sizeof(FileBlock));

//         file.seekp(pointerPos);
//         file.write(reinterpret_cast<char*>(&pb),
//                    sizeof(PointerBlock));
//     }

//     inode.i_size = content.size();

//     return true;
// }

// }

#include "BlockManager.h"
#include <vector>
#include <cstring>
#include <iostream>

namespace BlockManager {

// ======================================================
// ================= SUPERBLOCK UPDATE ==================
// ======================================================

void UpdateSuperBlock(std::fstream& file, int partitionStart, SuperBlock& sb)
{
    file.seekp(partitionStart);
    file.write(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));
}

// ======================================================
// ================= ALLOCATE BLOCK =====================
// ======================================================

int AllocateBlock(std::fstream& file, SuperBlock& sb, int partitionStart)
{
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

            for(int j = 0; j < sb.s_blocks_count; j++)
            {
                if(bitmap[j] == '0')
                {
                    sb.s_first_blo = j;
                    break;
                }
            }

            UpdateSuperBlock(file, partitionStart, sb);
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
    if(blockIndex < 0) return;

    std::vector<char> bitmap(sb.s_blocks_count);

    file.seekg(sb.s_bm_block_start);
    file.read(bitmap.data(), sb.s_blocks_count);

    if(bitmap[blockIndex] == '1')
    {
        bitmap[blockIndex] = '0';

        file.seekp(sb.s_bm_block_start);
        file.write(bitmap.data(), sb.s_blocks_count);

        sb.s_free_blocks_count++;

        if(blockIndex < sb.s_first_blo)
            sb.s_first_blo = blockIndex;

        UpdateSuperBlock(file, partitionStart, sb);
    }
}

// ======================================================
// ================= READ FILE ==========================
// ======================================================

std::string ReadFileContent(std::fstream& file,
                            SuperBlock& sb,
                            Inode& inode)
{
    std::string content;

    int blocksNeeded =
        (inode.i_size + sizeof(FileBlock) - 1) / sizeof(FileBlock);

    int blocksRead = 0;

    // ================= DIRECTOS =================
    for(int i = 0; i < 12 && blocksRead < blocksNeeded; i++)
    {
        if(inode.i_block[i] == -1) break;

        FileBlock block{};
        int blockPos = sb.s_block_start +
                       (inode.i_block[i] * sizeof(FileBlock));

        file.seekg(blockPos);
        file.read(reinterpret_cast<char*>(&block), sizeof(FileBlock));

        content.append(block.b_content, sizeof(FileBlock));
        blocksRead++;
    }

    // ================= INDIRECTO SIMPLE =================
    if(blocksRead < blocksNeeded && inode.i_block[12] != -1)
    {
        PointerBlock pb{};
        int pointerPos = sb.s_block_start +
                         (inode.i_block[12] * sizeof(FileBlock));

        file.seekg(pointerPos);
        file.read(reinterpret_cast<char*>(&pb), sizeof(PointerBlock));

        for(int i = 0; i < 16 && blocksRead < blocksNeeded; i++)
        {
            if(pb.b_pointers[i] == -1) break;

            FileBlock block{};
            int blockPos = sb.s_block_start +
                           (pb.b_pointers[i] * sizeof(FileBlock));

            file.seekg(blockPos);
            file.read(reinterpret_cast<char*>(&block), sizeof(FileBlock));

            content.append(block.b_content, sizeof(FileBlock));
            blocksRead++;
        }
    }

    // ================= DOBLE INDIRECTO =================
    if(blocksRead < blocksNeeded && inode.i_block[13] != -1)
    {
        PointerBlock level1{};
        int level1Pos = sb.s_block_start +
                        (inode.i_block[13] * sizeof(FileBlock));

        file.seekg(level1Pos);
        file.read(reinterpret_cast<char*>(&level1), sizeof(PointerBlock));

        for(int i = 0; i < 16 && blocksRead < blocksNeeded; i++)
        {
            if(level1.b_pointers[i] == -1) continue;

            PointerBlock level2{};
            int level2Pos = sb.s_block_start +
                            (level1.b_pointers[i] * sizeof(FileBlock));

            file.seekg(level2Pos);
            file.read(reinterpret_cast<char*>(&level2), sizeof(PointerBlock));

            for(int j = 0; j < 16 && blocksRead < blocksNeeded; j++)
            {
                if(level2.b_pointers[j] == -1) continue;

                FileBlock block{};
                int blockPos = sb.s_block_start +
                               (level2.b_pointers[j] * sizeof(FileBlock));

                file.seekg(blockPos);
                file.read(reinterpret_cast<char*>(&block), sizeof(FileBlock));

                content.append(block.b_content, sizeof(FileBlock));
                blocksRead++;
            }
        }
    }

    content.resize(inode.i_size);
    return content;
}

// ======================================================
// ================= WRITE FILE =========================
// ======================================================

bool WriteFileContent(std::fstream& file,
                      SuperBlock& sb,
                      int partitionStart,
                      Inode& inode,
                      const std::string& content)
{
    int blocksNeeded =
        (content.size() + sizeof(FileBlock) - 1) / sizeof(FileBlock);

    if(blocksNeeded > 284) // 12 + 16 + 256
    {
        std::cout << "Error: Archivo excede capacidad Fase 3 (doble indirecto)\n";
        return false;
    }

    // ================= LIBERAR BLOQUES ANTERIORES =================

    // Directos
    for(int i = 0; i < 12; i++)
    {
        if(inode.i_block[i] != -1)
        {
            FreeBlock(file, sb, partitionStart, inode.i_block[i]);
            inode.i_block[i] = -1;
        }
    }

    // Simple
    if(inode.i_block[12] != -1)
    {
        PointerBlock pb{};
        int pointerPos = sb.s_block_start +
                         (inode.i_block[12] * sizeof(FileBlock));

        file.seekg(pointerPos);
        file.read(reinterpret_cast<char*>(&pb), sizeof(PointerBlock));

        for(int i = 0; i < 16; i++)
            if(pb.b_pointers[i] != -1)
                FreeBlock(file, sb, partitionStart, pb.b_pointers[i]);

        FreeBlock(file, sb, partitionStart, inode.i_block[12]);
        inode.i_block[12] = -1;
    }

    // Doble
    if(inode.i_block[13] != -1)
    {
        PointerBlock level1{};
        int level1Pos = sb.s_block_start +
                        (inode.i_block[13] * sizeof(FileBlock));

        file.seekg(level1Pos);
        file.read(reinterpret_cast<char*>(&level1), sizeof(PointerBlock));

        for(int i = 0; i < 16; i++)
        {
            if(level1.b_pointers[i] == -1) continue;

            PointerBlock level2{};
            int level2Pos = sb.s_block_start +
                            (level1.b_pointers[i] * sizeof(FileBlock));

            file.seekg(level2Pos);
            file.read(reinterpret_cast<char*>(&level2), sizeof(PointerBlock));

            for(int j = 0; j < 16; j++)
                if(level2.b_pointers[j] != -1)
                    FreeBlock(file, sb, partitionStart, level2.b_pointers[j]);

            FreeBlock(file, sb, partitionStart, level1.b_pointers[i]);
        }

        FreeBlock(file, sb, partitionStart, inode.i_block[13]);
        inode.i_block[13] = -1;
    }

    // ================= ASIGNAR NUEVOS BLOQUES =================

    int contentOffset = 0;

    // ---------- DIRECTOS ----------
    for(int i = 0; i < blocksNeeded && i < 12; i++)
    {
        int newBlock = AllocateBlock(file, sb, partitionStart);
        if(newBlock == -1) return false;

        inode.i_block[i] = newBlock;

        FileBlock block{};
        memset(block.b_content, 0, sizeof(block.b_content));

        int sizeToCopy =
            std::min((int)sizeof(FileBlock),
                     (int)content.size() - contentOffset);

        memcpy(block.b_content,
               content.c_str() + contentOffset,
               sizeToCopy);

        int blockPos = sb.s_block_start +
                       (newBlock * sizeof(FileBlock));

        file.seekp(blockPos);
        file.write(reinterpret_cast<char*>(&block), sizeof(FileBlock));

        contentOffset += sizeToCopy;
    }

    // ---------- SIMPLE ----------
    if(blocksNeeded > 12)
    {
        int pointerBlockIndex = AllocateBlock(file, sb, partitionStart);
        if(pointerBlockIndex == -1) return false;

        inode.i_block[12] = pointerBlockIndex;

        PointerBlock pb{};
        for(int i = 0; i < 16; i++) pb.b_pointers[i] = -1;

        int remaining = blocksNeeded - 12;

        for(int i = 0; i < 16 && remaining > 0; i++)
        {
            int newBlock = AllocateBlock(file, sb, partitionStart);
            if(newBlock == -1) return false;

            pb.b_pointers[i] = newBlock;

            FileBlock block{};
            memset(block.b_content, 0, sizeof(block.b_content));

            int sizeToCopy =
                std::min((int)sizeof(FileBlock),
                         (int)content.size() - contentOffset);

            memcpy(block.b_content,
                   content.c_str() + contentOffset,
                   sizeToCopy);

            int blockPos = sb.s_block_start +
                           (newBlock * sizeof(FileBlock));

            file.seekp(blockPos);
            file.write(reinterpret_cast<char*>(&block), sizeof(FileBlock));

            contentOffset += sizeToCopy;
            remaining--;
        }

        int pointerPos = sb.s_block_start +
                         (pointerBlockIndex * sizeof(FileBlock));

        file.seekp(pointerPos);
        file.write(reinterpret_cast<char*>(&pb), sizeof(PointerBlock));
    }

    // ---------- DOBLE ----------
    if(blocksNeeded > 28)
    {
        int level1Index = AllocateBlock(file, sb, partitionStart);
        if(level1Index == -1) return false;

        inode.i_block[13] = level1Index;

        PointerBlock level1{};
        for(int i = 0; i < 16; i++) level1.b_pointers[i] = -1;

        int remaining = blocksNeeded - 28;

        for(int i = 0; i < 16 && remaining > 0; i++)
        {
            int level2Index = AllocateBlock(file, sb, partitionStart);
            if(level2Index == -1) return false;

            level1.b_pointers[i] = level2Index;

            PointerBlock level2{};
            for(int j = 0; j < 16; j++) level2.b_pointers[j] = -1;

            for(int j = 0; j < 16 && remaining > 0; j++)
            {
                int newBlock = AllocateBlock(file, sb, partitionStart);
                if(newBlock == -1) return false;

                level2.b_pointers[j] = newBlock;

                FileBlock block{};
                memset(block.b_content, 0, sizeof(block.b_content));

                int sizeToCopy =
                    std::min((int)sizeof(FileBlock),
                             (int)content.size() - contentOffset);

                memcpy(block.b_content,
                       content.c_str() + contentOffset,
                       sizeToCopy);

                int blockPos = sb.s_block_start +
                               (newBlock * sizeof(FileBlock));

                file.seekp(blockPos);
                file.write(reinterpret_cast<char*>(&block), sizeof(FileBlock));

                contentOffset += sizeToCopy;
                remaining--;
            }

            int level2Pos = sb.s_block_start +
                            (level2Index * sizeof(FileBlock));

            file.seekp(level2Pos);
            file.write(reinterpret_cast<char*>(&level2), sizeof(PointerBlock));
        }

        int level1Pos = sb.s_block_start +
                        (level1Index * sizeof(FileBlock));

        file.seekp(level1Pos);
        file.write(reinterpret_cast<char*>(&level1), sizeof(PointerBlock));
    }

    inode.i_size = content.size();
    return true;
}

}