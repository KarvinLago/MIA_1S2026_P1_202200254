#pragma once
#include <fstream>
#include <string>
#include "../Structs/Structs.h"

namespace FileUtils {

bool CreateDiskFile(const std::string& path);
std::fstream OpenFile(const std::string& path);

// Busca partición por nombre en MBR y EBR
// Retorna true si encontró, y rellena outStart y outSize
bool FindPartition(std::fstream& file,
                   const std::string& name,
                   int& outStart,
                   int& outSize);

}