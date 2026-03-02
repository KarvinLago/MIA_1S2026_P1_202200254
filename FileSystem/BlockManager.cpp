#include "BlockManager.h"
#include <vector>
#include <cstring>
#include <iostream>
#include <algorithm>

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
// Nota: Lee y escribe bitmap en disco. NO actualiza SB.
// El caller debe llamar UpdateSuperBlock al final.
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

            // Escribir bitmap actualizado
            file.seekp(sb.s_bm_block_start);
            file.write(bitmap.data(), sb.s_blocks_count);

            // Actualizar SB en memoria
            sb.s_free_blocks_count--;

            // Buscar siguiente libre
            sb.s_first_blo = sb.s_blocks_count; // default = sin libre
            for(int j = i + 1; j < sb.s_blocks_count; j++)
            {
                if(bitmap[j] == '0')
                {
                    sb.s_first_blo = j;
                    break;
                }
            }

            return i;
        }
    }

    return -1;
}

// ======================================================
// ================= FREE BLOCK =========================
// Nota: Solo libera en bitmap. NO actualiza SB en disco.
// El caller debe llamar UpdateSuperBlock al final.
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

        if(blockIndex < sb.s_first_blo)
            sb.s_first_blo = blockIndex;
    }
}

// ======================================================
// ================= READ FILE ==========================
// Lee el contenido completo de un archivo desde su inodo.
// Maneja: 12 directos + simple + doble indirecto.
// ======================================================

std::string ReadFileContent(std::fstream& file,
                            SuperBlock& sb,
                            Inode& inode)
{
    std::string content;

    if(inode.i_size <= 0) return content;

    int bytesRemaining = inode.i_size;
    int blockSize      = sizeof(FileBlock); // 64 bytes

    auto readDataBlock = [&](int blockIndex)
    {
        if(bytesRemaining <= 0) return;
        if(blockIndex < 0 || blockIndex >= sb.s_blocks_count) return;

        FileBlock block{};
        int blockPos = sb.s_block_start + (blockIndex * blockSize);

        file.seekg(blockPos);
        file.read(reinterpret_cast<char*>(&block), blockSize);

        int toCopy = std::min(bytesRemaining, blockSize);
        content.append(block.b_content, toCopy);
        bytesRemaining -= toCopy;
    };

    // ================= DIRECTOS (0..11) =================
    for(int i = 0; i < 12 && bytesRemaining > 0; i++)
    {
        if(inode.i_block[i] == -1) break;
        readDataBlock(inode.i_block[i]);
    }

    // ================= SIMPLE INDIRECTO (12) =================
    if(bytesRemaining > 0 && inode.i_block[12] != -1)
    {
        PointerBlock pb{};
        int pointerPos = sb.s_block_start + (inode.i_block[12] * blockSize);

        file.seekg(pointerPos);
        file.read(reinterpret_cast<char*>(&pb), sizeof(PointerBlock));

        for(int i = 0; i < 16 && bytesRemaining > 0; i++)
        {
            if(pb.b_pointers[i] == -1) break;
            readDataBlock(pb.b_pointers[i]);
        }
    }

    // ================= DOBLE INDIRECTO (13) =================
    if(bytesRemaining > 0 && inode.i_block[13] != -1)
    {
        PointerBlock level1{};
        int level1Pos = sb.s_block_start + (inode.i_block[13] * blockSize);

        file.seekg(level1Pos);
        file.read(reinterpret_cast<char*>(&level1), sizeof(PointerBlock));

        for(int i = 0; i < 16 && bytesRemaining > 0; i++)
        {
            if(level1.b_pointers[i] == -1) break;

            PointerBlock level2{};
            int level2Pos = sb.s_block_start + (level1.b_pointers[i] * blockSize);

            file.seekg(level2Pos);
            file.read(reinterpret_cast<char*>(&level2), sizeof(PointerBlock));

            for(int j = 0; j < 16 && bytesRemaining > 0; j++)
            {
                if(level2.b_pointers[j] == -1) break;
                readDataBlock(level2.b_pointers[j]);
            }
        }
    }

    // ================= TRIPLE INDIRECTO (14) =================
    if(bytesRemaining > 0 && inode.i_block[14] != -1)
    {
        PointerBlock level1{};
        int level1Pos = sb.s_block_start + (inode.i_block[14] * blockSize);

        file.seekg(level1Pos);
        file.read(reinterpret_cast<char*>(&level1), sizeof(PointerBlock));

        for(int i = 0; i < 16 && bytesRemaining > 0; i++)
        {
            if(level1.b_pointers[i] == -1) break;

            PointerBlock level2{};
            int level2Pos = sb.s_block_start + (level1.b_pointers[i] * blockSize);

            file.seekg(level2Pos);
            file.read(reinterpret_cast<char*>(&level2), sizeof(PointerBlock));

            for(int j = 0; j < 16 && bytesRemaining > 0; j++)
            {
                if(level2.b_pointers[j] == -1) break;

                PointerBlock level3{};
                int level3Pos = sb.s_block_start + (level2.b_pointers[j] * blockSize);

                file.seekg(level3Pos);
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
// Escribe contenido completo en el inodo.
// Libera bloques anteriores, asigna nuevos.
// Actualiza inode.i_size e inode.i_block[].
// IMPORTANTE: El caller debe escribir el inode al disco
//             después de llamar a esta función.
// ======================================================

bool WriteFileContent(std::fstream& file,
                      SuperBlock& sb,
                      int partitionStart,
                      Inode& inode,
                      const std::string& content)
{
    int blockSize = sizeof(FileBlock); // 64 bytes

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

    // Simple indirecto
    if(inode.i_block[12] != -1)
    {
        PointerBlock pb{};
        int pointerPos = sb.s_block_start + (inode.i_block[12] * blockSize);

        file.seekg(pointerPos);
        file.read(reinterpret_cast<char*>(&pb), sizeof(PointerBlock));

        for(int i = 0; i < 16; i++)
            if(pb.b_pointers[i] != -1)
                FreeBlock(file, sb, partitionStart, pb.b_pointers[i]);

        FreeBlock(file, sb, partitionStart, inode.i_block[12]);
        inode.i_block[12] = -1;
    }

    // Doble indirecto
    if(inode.i_block[13] != -1)
    {
        PointerBlock level1{};
        int level1Pos = sb.s_block_start + (inode.i_block[13] * blockSize);

        file.seekg(level1Pos);
        file.read(reinterpret_cast<char*>(&level1), sizeof(PointerBlock));

        for(int i = 0; i < 16; i++)
        {
            if(level1.b_pointers[i] == -1) continue;

            PointerBlock level2{};
            int level2Pos = sb.s_block_start + (level1.b_pointers[i] * blockSize);

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

    // Triple indirecto
    if(inode.i_block[14] != -1)
    {
        PointerBlock level1{};
        int level1Pos = sb.s_block_start + (inode.i_block[14] * blockSize);

        file.seekg(level1Pos);
        file.read(reinterpret_cast<char*>(&level1), sizeof(PointerBlock));

        for(int i = 0; i < 16; i++)
        {
            if(level1.b_pointers[i] == -1) continue;

            PointerBlock level2{};
            int level2Pos = sb.s_block_start + (level1.b_pointers[i] * blockSize);

            file.seekg(level2Pos);
            file.read(reinterpret_cast<char*>(&level2), sizeof(PointerBlock));

            for(int j = 0; j < 16; j++)
            {
                if(level2.b_pointers[j] == -1) continue;

                PointerBlock level3{};
                int level3Pos = sb.s_block_start + (level2.b_pointers[j] * blockSize);

                file.seekg(level3Pos);
                file.read(reinterpret_cast<char*>(&level3), sizeof(PointerBlock));

                for(int k = 0; k < 16; k++)
                    if(level3.b_pointers[k] != -1)
                        FreeBlock(file, sb, partitionStart, level3.b_pointers[k]);

                FreeBlock(file, sb, partitionStart, level2.b_pointers[j]);
            }

            FreeBlock(file, sb, partitionStart, level1.b_pointers[i]);
        }

        FreeBlock(file, sb, partitionStart, inode.i_block[14]);
        inode.i_block[14] = -1;
    }

    // ================= CALCULAR BLOQUES NECESARIOS =================

    int contentSize  = (int)content.size();
    int blocksNeeded = (contentSize + blockSize - 1) / blockSize;

    // Capacidad máxima: 12 + 16 + 256 + 4096 = 4380 bloques
    int maxBlocks = 12 + 16 + (16*16) + (16*16*16);
    if(blocksNeeded > maxBlocks)
    {
        std::cout << "Error: Contenido excede capacidad máxima del sistema EXT2\n";
        return false;
    }

    if(blocksNeeded > sb.s_free_blocks_count)
    {
        std::cout << "Error: No hay suficientes bloques libres (necesarios: "
                  << blocksNeeded << ", libres: " << sb.s_free_blocks_count << ")\n";
        return false;
    }

    // ================= ESCRITURA =================

    int remaining     = blocksNeeded;
    int contentOffset = 0;

    // Lambda para escribir un bloque de datos
    auto writeDataBlock = [&](int blockIndex) -> bool
    {
        FileBlock block{};
        memset(block.b_content, 0, blockSize);

        int sizeToCopy = std::min(blockSize, contentSize - contentOffset);
        if(sizeToCopy > 0)
            memcpy(block.b_content, content.c_str() + contentOffset, sizeToCopy);

        int blockPos = sb.s_block_start + (blockIndex * blockSize);
        file.seekp(blockPos);
        file.write(reinterpret_cast<char*>(&block), blockSize);

        contentOffset += sizeToCopy;
        remaining--;
        return true;
    };

    // -------- DIRECTOS (i_block[0..11]) --------
    for(int i = 0; i < 12 && remaining > 0; i++)
    {
        int newBlock = AllocateBlock(file, sb, partitionStart);
        if(newBlock == -1) return false;

        inode.i_block[i] = newBlock;
        writeDataBlock(newBlock);
    }

    // -------- SIMPLE INDIRECTO (i_block[12]) --------
    if(remaining > 0)
    {
        int pbIndex = AllocateBlock(file, sb, partitionStart);
        if(pbIndex == -1) return false;

        inode.i_block[12] = pbIndex;

        PointerBlock pb{};
        for(int i = 0; i < 16; i++) pb.b_pointers[i] = -1;

        for(int i = 0; i < 16 && remaining > 0; i++)
        {
            int newBlock = AllocateBlock(file, sb, partitionStart);
            if(newBlock == -1) return false;

            pb.b_pointers[i] = newBlock;
            writeDataBlock(newBlock);
        }

        int pbPos = sb.s_block_start + (pbIndex * blockSize);
        file.seekp(pbPos);
        file.write(reinterpret_cast<char*>(&pb), sizeof(PointerBlock));
    }

    // -------- DOBLE INDIRECTO (i_block[13]) --------
    if(remaining > 0)
    {
        int l1Index = AllocateBlock(file, sb, partitionStart);
        if(l1Index == -1) return false;

        inode.i_block[13] = l1Index;

        PointerBlock level1{};
        for(int i = 0; i < 16; i++) level1.b_pointers[i] = -1;

        for(int i = 0; i < 16 && remaining > 0; i++)
        {
            int l2Index = AllocateBlock(file, sb, partitionStart);
            if(l2Index == -1) return false;

            level1.b_pointers[i] = l2Index;

            PointerBlock level2{};
            for(int j = 0; j < 16; j++) level2.b_pointers[j] = -1;

            for(int j = 0; j < 16 && remaining > 0; j++)
            {
                int newBlock = AllocateBlock(file, sb, partitionStart);
                if(newBlock == -1) return false;

                level2.b_pointers[j] = newBlock;
                writeDataBlock(newBlock);
            }

            int l2Pos = sb.s_block_start + (l2Index * blockSize);
            file.seekp(l2Pos);
            file.write(reinterpret_cast<char*>(&level2), sizeof(PointerBlock));
        }

        int l1Pos = sb.s_block_start + (l1Index * blockSize);
        file.seekp(l1Pos);
        file.write(reinterpret_cast<char*>(&level1), sizeof(PointerBlock));
    }

    // -------- TRIPLE INDIRECTO (i_block[14]) --------
    if(remaining > 0)
    {
        int l1Index = AllocateBlock(file, sb, partitionStart);
        if(l1Index == -1) return false;

        inode.i_block[14] = l1Index;

        PointerBlock level1{};
        for(int i = 0; i < 16; i++) level1.b_pointers[i] = -1;

        for(int i = 0; i < 16 && remaining > 0; i++)
        {
            int l2Index = AllocateBlock(file, sb, partitionStart);
            if(l2Index == -1) return false;

            level1.b_pointers[i] = l2Index;

            PointerBlock level2{};
            for(int j = 0; j < 16; j++) level2.b_pointers[j] = -1;

            for(int j = 0; j < 16 && remaining > 0; j++)
            {
                int l3Index = AllocateBlock(file, sb, partitionStart);
                if(l3Index == -1) return false;

                level2.b_pointers[j] = l3Index;

                PointerBlock level3{};
                for(int k = 0; k < 16; k++) level3.b_pointers[k] = -1;

                for(int k = 0; k < 16 && remaining > 0; k++)
                {
                    int newBlock = AllocateBlock(file, sb, partitionStart);
                    if(newBlock == -1) return false;

                    level3.b_pointers[k] = newBlock;
                    writeDataBlock(newBlock);
                }

                int l3Pos = sb.s_block_start + (l3Index * blockSize);
                file.seekp(l3Pos);
                file.write(reinterpret_cast<char*>(&level3), sizeof(PointerBlock));
            }

            int l2Pos = sb.s_block_start + (l2Index * blockSize);
            file.seekp(l2Pos);
            file.write(reinterpret_cast<char*>(&level2), sizeof(PointerBlock));
        }

        int l1Pos = sb.s_block_start + (l1Index * blockSize);
        file.seekp(l1Pos);
        file.write(reinterpret_cast<char*>(&level1), sizeof(PointerBlock));
    }

    // ================= ACTUALIZAR INODE Y SUPERBLOCK =================

    inode.i_size  = contentSize;
    inode.i_mtime = time(nullptr);

    // Escribir SuperBlock actualizado al disco
    UpdateSuperBlock(file, partitionStart, sb);

    return true;
}

} // namespace BlockManager