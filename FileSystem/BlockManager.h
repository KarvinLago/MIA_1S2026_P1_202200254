#pragma once
#include <fstream>
#include <string>
#include "../Structs/Structs.h"

namespace BlockManager {

// ================= SUPERBLOCK =================
void UpdateSuperBlock(std::fstream& file, int partitionStart, SuperBlock& sb);

// ================= BITMAP BLOQUES =================
int  AllocateBlock(std::fstream& file, SuperBlock& sb, int partitionStart);
void FreeBlock(std::fstream& file, SuperBlock& sb, int partitionStart, int blockIndex);

// ================= BITMAP INODOS =================
int  AllocateInode(std::fstream& file, SuperBlock& sb, int partitionStart);
void FreeInode(std::fstream& file, SuperBlock& sb, int partitionStart, int inodeIndex);

// ================= INODE R/W =================
Inode ReadInode(std::fstream& file, SuperBlock& sb, int inodeIndex);
void  WriteInode(std::fstream& file, SuperBlock& sb, int inodeIndex, Inode& inode);

// ================= FILE ENGINE =================
std::string ReadFileContent(std::fstream& file, SuperBlock& sb, Inode& inode);
bool        WriteFileContent(std::fstream& file, SuperBlock& sb, int partitionStart,
                             Inode& inode, const std::string& content);

// ================= FOLDER ENGINE =================
// Agrega una entrada (nombre + inodoIndex) en la carpeta apuntada por folderInode.
// Retorna true si tuvo éxito.
bool AddEntryToFolder(std::fstream& file, SuperBlock& sb, int partitionStart,
                      Inode& folderInode, int folderInodeIndex,
                      const std::string& entryName, int entryInodeIndex);

} // namespace BlockManager