#pragma once
#include <fstream>
#include <string>
#include "../Structs/Structs.h"

namespace BlockManager {

// ================= BITMAP ENGINE =================

int AllocateBlock(std::fstream& file, SuperBlock& sb, int partitionStart);
void FreeBlock(std::fstream& file, SuperBlock& sb, int partitionStart, int blockIndex);

void UpdateSuperBlock(std::fstream& file, int partitionStart, SuperBlock& sb);

// ================= FILE ENGINE =================

std::string ReadFileContent(std::fstream& file,
                            SuperBlock& sb,
                            Inode& inode);

bool WriteFileContent(std::fstream& file,
                      SuperBlock& sb,
                      int partitionStart,
                      Inode& inode,
                      const std::string& content);

}