#include "FileUtils.h"
#include <filesystem>

namespace FileUtils {

bool CreateDiskFile(const std::string& path) {
    std::filesystem::create_directories(
        std::filesystem::path(path).parent_path()
    );
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    return file.good();
}

std::fstream OpenFile(const std::string& path) {
    return std::fstream(path, std::ios::in | std::ios::out | std::ios::binary);
}

bool FindPartition(std::fstream& file,
                   const std::string& name,
                   int& outStart,
                   int& outSize)
{
    MBR mbr{};
    file.seekg(0);
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

    // Buscar en particiones primarias/extendidas del MBR
    for(int i = 0; i < 4; i++){
        if(mbr.mbr_partitions[i].part_s > 0 &&
           std::string(mbr.mbr_partitions[i].part_name) == name){
            outStart = mbr.mbr_partitions[i].part_start;
            outSize  = mbr.mbr_partitions[i].part_s;
            return true;
        }
    }

    // Buscar en EBR dentro de la partición extendida
    for(int i = 0; i < 4; i++){
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
                if(std::string(ebr.part_name) == name){
                    outStart = ebr.part_start;
                    outSize  = ebr.part_s;
                    return true;
                }
                ebrPos = ebr.part_next;
            }
            break;
        }
    }
    return false;
}

} // namespace FileUtils