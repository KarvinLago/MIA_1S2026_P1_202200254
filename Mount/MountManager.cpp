#include "MountManager.h"
#include "../Structs/Structs.h"
#include "../Utilities/FileUtils.h"
#include <iostream>
#include <algorithm>

// static std::vector<MountedPartition> mountedList;
std::vector<MountedPartition> mountedList;
// Carnet completo para generar ID
static std::string fullCarnet = "202200254";

namespace MountManager {

void Mount(const std::string& path,
           const std::string& name)
{
    auto file = FileUtils::OpenFile(path);
    if(!file.is_open()){
        std::cout << "Error: No se pudo abrir el disco\n";
        return;
    }

    MBR mbr{};
    file.seekg(0);
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
    file.close();

    // ================= VERIFICAR QUE LA PARTICIÓN EXISTE =================
    bool found = false;

    for(int i = 0; i < 4; i++){
        if(mbr.mbr_partitions[i].part_s > 0){
            if(std::string(mbr.mbr_partitions[i].part_name) == name){
                found = true;
                break;
            }
        }
    }

    if(!found){
        std::cout << "Error: Partición no encontrada\n";
        return;
    }

    // ================= VERIFICAR QUE NO ESTÉ YA MONTADA =================
    for(const auto& m : mountedList){
        if(m.path == path && m.name == name){
            std::cout << "Error: Partición ya montada\n";
            return;
        }
    }

    // ================= GENERAR LETRA DEL DISCO =================
    char letter = 0;

    // Si el disco ya tiene letra asignada, reutilizarla
    for(const auto& m : mountedList){
        if(m.path == path){
            letter = m.id.back(); // última letra del ID
            break;
        }
    }

    // Si es un disco nuevo, asignar nueva letra
    if(letter == 0){
        letter = 'A';

        // Buscar letras ya usadas
        for(const auto& m : mountedList){
            if(m.id.back() >= letter){
                letter = m.id.back() + 1;
            }
        }
    }

    // ================= GENERAR NÚMERO CORRELATIVO =================
    int count = 1;
    for(const auto& m : mountedList){
        if(m.path == path){
            count++;
        }
    }

    // ================= GENERAR ID FINAL =================
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

}