#include "LoginManager.h"
#include "../Mount/MountManager.h"
#include "../Structs/Structs.h"
#include "../Utilities/FileUtils.h"
#include "../FileSystem/BlockManager.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <cstring>

extern std::vector<MountedPartition> mountedList;

struct Session {
    bool        active = false;
    std::string user;
    std::string id;
    int         uid = 0;
    int         gid = 0;
};

static Session currentSession;

static bool FindPartition(std::fstream& file,
                          const std::string& name,
                          int& outStart,
                          int& outSize)
{
    MBR mbr{};
    file.seekg(0);
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

    for(int i = 0; i < 4; i++){
        if(mbr.mbr_partitions[i].part_s > 0 &&
           std::string(mbr.mbr_partitions[i].part_name) == name){
            outStart = mbr.mbr_partitions[i].part_start;
            outSize  = mbr.mbr_partitions[i].part_s;
            return true;
        }
    }

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

namespace LoginManager {

void Login(const std::string& user,
           const std::string& pass,
           const std::string& id)
{
    if(currentSession.active){
        std::cout << "Error: Ya existe una sesión activa\n";
        return;
    }

    MountedPartition* mounted = nullptr;
    for(auto& m : mountedList)
        if(m.id == id){ mounted = &m; break; }

    if(!mounted){
        std::cout << "Error: ID no encontrado\n";
        return;
    }

    auto file = FileUtils::OpenFile(mounted->path);
    if(!file.is_open()){
        std::cout << "Error: No se pudo abrir el disco\n";
        return;
    }

    int partStart = -1, partSize = -1;
    if(!FindPartition(file, mounted->name, partStart, partSize)){
        std::cout << "Error: Partición no encontrada\n";
        file.close();
        return;
    }

    SuperBlock sb{};
    file.seekg(partStart);
    file.read(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));

    Inode usersInode{};
    file.seekg(sb.s_inode_start + sizeof(Inode));
    file.read(reinterpret_cast<char*>(&usersInode), sizeof(Inode));

    std::string content = BlockManager::ReadFileContent(file, sb, usersInode);
    file.close();

    std::stringstream ss(content);
    std::string line;
    bool valid = false;
    int foundUid = 0, foundGid = 0;

    while(std::getline(ss, line))
    {
        if(line.empty()) continue;
        std::vector<std::string> tokens;
        std::stringstream ls(line);
        std::string token;
        while(std::getline(ls, token, ',')) tokens.push_back(token);

        if(tokens.size() == 5 && tokens[1] == "U" &&
           tokens[0] != "0"   && tokens[3] == user &&
           tokens[4] == pass)
        {
            foundUid = std::stoi(tokens[0]);
            std::stringstream ss2(content);
            std::string line2;
            while(std::getline(ss2, line2))
            {
                if(line2.empty()) continue;
                std::vector<std::string> t2;
                std::stringstream ls2(line2);
                std::string tok2;
                while(std::getline(ls2, tok2, ',')) t2.push_back(tok2);
                if(t2.size() == 3 && t2[1] == "G" &&
                   t2[0] != "0"   && t2[2] == tokens[2])
                {
                    foundGid = std::stoi(t2[0]);
                    break;
                }
            }
            valid = true;
            break;
        }
    }

    if(valid){
        currentSession.active = true;
        currentSession.user   = user;
        currentSession.id     = id;
        currentSession.uid    = foundUid;
        currentSession.gid    = foundGid;
        std::cout << "Login exitoso\n";
    } else {
        std::cout << "Error: Credenciales incorrectas\n";
    }
}

void Logout()
{
    if(!currentSession.active){
        std::cout << "Error: No hay sesión activa\n";
        return;
    }
    currentSession = Session{};
    std::cout << "Logout exitoso\n";
}

bool IsLogged()             { return currentSession.active; }
bool IsRoot()               { return currentSession.active && currentSession.user == "root"; }
std::string GetSessionId()  { return currentSession.id; }
std::string GetUser()       { return currentSession.user; }
int GetSessionUid()         { return currentSession.uid; }
int GetSessionGid()         { return currentSession.gid; }

} // namespace LoginManager
