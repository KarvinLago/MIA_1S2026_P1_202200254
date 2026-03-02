#pragma once
#include <string>
#include <vector>

namespace CatManager {

// Ejecuta el comando CAT con una lista de rutas de archivos
void Cat(const std::vector<std::string>& paths);

}