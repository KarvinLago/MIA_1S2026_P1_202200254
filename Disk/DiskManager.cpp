#include "DiskManager.h"
#include "../Structs/Structs.h"
#include "../Utilities/FileUtils.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <ctime>
#include <cstdlib>

namespace DiskManager {

void Mkdisk(int size, char fit, char unit, const std::string& path) {

    if(size <= 0){
        std::cout << "Error: Tamaño inválido\n";
        return;
    }

    if(unit == 'k') size *= 1024;
    else if(unit == 'm') size *= 1024 * 1024;
    else {
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

}