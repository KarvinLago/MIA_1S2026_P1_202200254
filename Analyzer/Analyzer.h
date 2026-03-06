// #pragma once

// namespace Analyzer {

//     void Analyze();

// }

#pragma once
#include <string>

namespace Analyzer {

// Modo consola (loop original)
void Analyze();

// Modo API: recibe una línea ya leída
void AnalyzeLine(const std::string& input);

}
