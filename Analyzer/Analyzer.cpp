#include "Analyzer.h"
#include "../Disk/DiskManager.h"
#include <iostream>
#include <sstream>
#include <map>
#include <regex>

namespace Analyzer {

void Analyze() {

    std::string input;
    std::getline(std::cin, input);

    std::istringstream iss(input);
    std::string command;
    iss >> command;

    if(command == "mkdisk"){

        std::regex re(R"(-(\w+)=("[^"]+"|\S+))");
        std::sregex_iterator it(input.begin(), input.end(), re);
        std::sregex_iterator end;

        std::map<std::string,std::string> params;

        for(; it != end; ++it){
            params[it->str(1)] = it->str(2);
        }

        int size = std::stoi(params["size"]);
        char fit = params["fit"][0];
        char unit = params["unit"][0];
        std::string path = params["path"];

        DiskManager::Mkdisk(size, fit, unit, path);
    }
}

}