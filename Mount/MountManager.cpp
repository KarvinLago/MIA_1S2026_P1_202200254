#include "MountManager.h"
#include "../Structs/Structs.h"
#include "../Utilities/FileUtils.h"
#include <iostream>
#include <algorithm>

std::vector<MountedPartition> mountedList;

static std::string fullCarnet = "202200254";

namespace MountManager {

void Mount(const std::string& path, const std::string& name)
{
    auto file = FileUtils::OpenFile(path);
    if(!file.is_open()){
        std::cout << "Error: No se pudo abrir el disco\n";
        return;
    }

    MBR mbr{};
    file.seekg(0);
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

    // ── Verificar que la partición existe (MBR o EBR) ──
    bool found = false;

    // Buscar en particiones primarias/extendidas del MBR
    for(int i = 0; i < 4; i++){
        if(mbr.mbr_partitions[i].part_s > 0 &&
           std::string(mbr.mbr_partitions[i].part_name) == name){
            found = true;
            break;
        }
    }

    // Si no encontró, buscar en EBR de la partición extendida
    if(!found)
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

                    if(std::string(ebr.part_name) == name){
                        found = true;
                        break;
                    }

                    ebrPos = ebr.part_next;
                }
                break;
            }
        }
    }

    file.close();

    if(!found){
        std::cout << "Error: Partición no encontrada\n";
        return;
    }

    // ── Verificar que no esté ya montada ──
    for(const auto& m : mountedList){
        if(m.path == path && m.name == name){
            std::cout << "Error: Partición ya montada\n";
            return;
        }
    }

    // ── Generar letra del disco ──
    char letter = 0;
    for(const auto& m : mountedList){
        if(m.path == path){
            letter = m.id.back();
            break;
        }
    }

    if(letter == 0){
        letter = 'A';
        for(const auto& m : mountedList){
            if(m.id.back() >= letter){
                letter = m.id.back() + 1;
            }
        }
    }

    // ── Generar número correlativo ──
    int count = 1;
    for(const auto& m : mountedList){
        if(m.path == path) count++;
    }

    // ── Generar ID final ──
    std::string lastTwo = fullCarnet.substr(fullCarnet.length() - 2);
    std::string id = lastTwo + std::to_string(count) + letter;

    mountedList.push_back({path, name, id});
    std::cout << "Partición montada correctamente. ID: " << id << "\n";
}

void ShowMounted()
{
    if(mountedList.empty()){
        std::cout << "No hay particiones montadas\n";
        return;
    }
    std::cout << "Particiones montadas:\n";
    for(const auto& m : mountedList){
        std::cout << "ID: " << m.id
                  << " | Path: " << m.path
                  << " | Name: " << m.name << "\n";
    }
}

} // namespace MountManager
