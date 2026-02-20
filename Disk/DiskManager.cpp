// #include "DiskManager.h"
// #include "../Structs/Structs.h"
// #include "../Utilities/FileUtils.h"
// #include <iostream>
// #include <vector>
// #include <cstring>
// #include <ctime>
// #include <cstdlib>

// namespace DiskManager {

// void Mkdisk(int size, char fit, char unit, const std::string& path) {

//     if(size <= 0){
//         std::cout << "Error: Tamaño inválido\n";
//         return;
//     }

//     if(unit == 'k') size *= 1024;
//     else if(unit == 'm') size *= 1024 * 1024;
//     else {
//         std::cout << "Error: Unidad inválida\n";
//         return;
//     }

//     if(!FileUtils::CreateDiskFile(path)){
//         std::cout << "Error creando archivo\n";
//         return;
//     }

//     auto file = FileUtils::OpenFile(path);

//     std::vector<char> buffer(1024, 0);

//     for(int i = 0; i < size / 1024; i++){
//         file.write(buffer.data(), 1024);
//     }

//     MBR mbr{};
//     mbr.mbr_tamano = size;
//     mbr.mbr_fecha_creacion = time(nullptr);
//     mbr.mbr_dsk_signature = rand();
//     mbr.dsk_fit = fit;

//     file.seekp(0);
//     file.write(reinterpret_cast<char*>(&mbr), sizeof(MBR));

//     file.close();

//     std::cout << "Disco creado correctamente\n";
// }

// }

#include "DiskManager.h"
#include "../Structs/Structs.h"
#include "../Utilities/FileUtils.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <algorithm>

namespace DiskManager {

// ========================= MKDISK =========================

void Mkdisk(int size, char fit, char unit, const std::string& path)
{
    if(size <= 0){
        std::cout << "Error: Tamaño inválido\n";
        return;
    }

    if(unit == 'k') size *= 1024;
    else if(unit == 'm') size *= 1024 * 1024;
    else{
        std::cout << "Error: Unidad inválida\n";
        return;
    }

    if(!FileUtils::CreateDiskFile(path)){
        std::cout << "Error creando archivo\n";
        return;
    }

    auto file = FileUtils::OpenFile(path);

    std::vector<char> buffer(1024, 0);
    for(int i = 0; i < size / 1024; i++){
        file.write(buffer.data(), 1024);
    }

    MBR mbr{};
    mbr.mbr_tamano = size;
    mbr.mbr_fecha_creacion = time(nullptr);
    mbr.mbr_dsk_signature = rand();
    mbr.dsk_fit = fit;

    file.seekp(0);
    file.write(reinterpret_cast<char*>(&mbr), sizeof(MBR));

    file.close();

    std::cout << "Disco creado correctamente\n";
}

// ========================= FDISK =========================

void Fdisk(int size,
           const std::string& path,
           const std::string& name,
           char type,
           char fit,
           char unit)
{
    if(size <= 0){
        std::cout << "Error: Tamaño inválido\n";
        return;
    }

    if(unit == 'k') size *= 1024;
    else if(unit == 'm') size *= 1024 * 1024;
    else if(unit != 'b'){
        std::cout << "Error: Unidad inválida\n";
        return;
    }

    auto file = FileUtils::OpenFile(path);
    if(!file.is_open()){
        std::cout << "Error: No se pudo abrir el disco\n";
        return;
    }

    MBR mbr{};
    file.seekg(0);
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

    // ===== VALIDAR MÁXIMO 4 PARTICIONES =====
    int index = -1;
    for(int i = 0; i < 4; i++){
        if(mbr.mbr_partitions[i].part_s == 0){
            index = i;
            break;
        }
    }

    if(index == -1){
        std::cout << "Error: Máximo 4 particiones alcanzado\n";
        file.close();
        return;
    }

    // ===== CALCULAR START CORRECTAMENTE =====
    int start = sizeof(MBR);

    for(int i = 0; i < 4; i++){
        if(mbr.mbr_partitions[i].part_s > 0){
            int end = mbr.mbr_partitions[i].part_start +
                      mbr.mbr_partitions[i].part_s;

            if(end > start){
                start = end;
            }
        }
    }

    // ===== VALIDAR ESPACIO REAL =====
    if(start + size > mbr.mbr_tamano){
        std::cout << "Error: No hay espacio suficiente en el disco\n";
        file.close();
        return;
    }

    // ===== CREAR PARTICIÓN =====
    Partition &p = mbr.mbr_partitions[index];

    p.part_status = '0';
    p.part_type = type;
    p.part_fit = fit;
    p.part_start = start;
    p.part_s = size;
    p.part_correlative = index + 1;

    std::memset(p.part_name, 0, 16);
    std::memcpy(p.part_name, name.c_str(),
                std::min((size_t)15, name.size()));

    file.seekp(0);
    file.write(reinterpret_cast<char*>(&mbr), sizeof(MBR));

    file.close();

    std::cout << "Partición creada correctamente\n";
}

}