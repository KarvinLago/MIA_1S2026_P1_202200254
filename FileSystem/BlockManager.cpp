#include "BlockManager.h"
#include <vector>
#include <cstring>
#include <cmath>
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

            // escribir bitmap actualizado
            file.seekp(sb.s_bm_block_start);
            file.write(bitmap.data(), sb.s_blocks_count);

            sb.s_free_blocks_count--;

            // actualizar s_first_blo
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

    return -1; // sin espacio
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

    int blocksNeeded = std::ceil((double)inode.i_size / sizeof(FileBlock));

    for(int i = 0; i < blocksNeeded && i < 12; i++)
    {
        if(inode.i_block[i] == -1) break;

        int blockPos = sb.s_block_start +
                       (inode.i_block[i] * sizeof(FileBlock));

        FileBlock block{};
        file.seekg(blockPos);
        file.read(reinterpret_cast<char*>(&block), sizeof(FileBlock));

        content.append(block.b_content, sizeof(FileBlock));
    }

    content.resize(inode.i_size);
    return content;
}

// ======================================================
// ================= WRITE FILE (DIRECTOS) ==============
// ======================================================

bool WriteFileContent(std::fstream& file,
                      SuperBlock& sb,
                      int partitionStart,
                      Inode& inode,
                      const std::string& content)
{
    int blocksNeeded =
        std::ceil((double)content.size() / sizeof(FileBlock));

    if(blocksNeeded > 12)
    {
        std::cout << "Error: Archivo excede 12 bloques directos (Fase 1)\n";
        return false;
    }

    // Liberar bloques anteriores
    for(int i = 0; i < 12; i++)
    {
        if(inode.i_block[i] != -1)
        {
            FreeBlock(file, sb, partitionStart, inode.i_block[i]);
            inode.i_block[i] = -1;
        }
    }

    // Asignar nuevos bloques
    for(int i = 0; i < blocksNeeded; i++)
    {
        int newBlock = AllocateBlock(file, sb, partitionStart);
        if(newBlock == -1)
        {
            std::cout << "Error: No hay bloques disponibles\n";
            return false;
        }

        inode.i_block[i] = newBlock;

        FileBlock block{};
        memset(block.b_content, 0, sizeof(block.b_content));

        int start = i * sizeof(FileBlock);
        int sizeToCopy =
            std::min((int)sizeof(FileBlock),
                     (int)content.size() - start);

        memcpy(block.b_content,
               content.c_str() + start,
               sizeToCopy);

        int blockPos = sb.s_block_start +
                       (newBlock * sizeof(FileBlock));

        file.seekp(blockPos);
        file.write(reinterpret_cast<char*>(&block),
                   sizeof(FileBlock));
    }

    inode.i_size = content.size();

    return true;
}

}