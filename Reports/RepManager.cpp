#include "RepManager.h"
#include "../FileSystem/BlockManager.h"
#include "../Mount/MountManager.h"
#include "../Structs/Structs.h"
#include "../Utilities/FileUtils.h"
#include "../Auth/LoginManager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <sys/stat.h>

extern std::vector<MountedPartition> mountedList;

namespace RepManager {

// ======================================================
// ================= UTILIDADES =========================
// ======================================================

static void CreateDirs(const std::string& path)
{
    std::string dir = path.substr(0, path.find_last_of('/'));
    if(dir.empty()) return;
    system(("mkdir -p \"" + dir + "\"").c_str());
}

static void RunDot(const std::string& dotFile, const std::string& outFile)
{
    std::string ext = outFile.substr(outFile.find_last_of('.') + 1);
    std::string fmt = (ext == "jpg" || ext == "jpeg") ? "jpg"
                    : (ext == "png") ? "png"
                    : (ext == "pdf") ? "pdf" : "jpg";
    system(("dot -T" + fmt + " \"" + dotFile + "\" -o \"" + outFile + "\"").c_str());
}

static std::string TimeToStr(time_t t)
{
    if(t == 0) return "N/A";
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&t));
    return std::string(buf);
}

static std::vector<std::string> SplitPath(const std::string& path)
{
    std::vector<std::string> parts;
    std::stringstream ss(path); std::string token;
    while(std::getline(ss, token, '/')) if(!token.empty()) parts.push_back(token);
    return parts;
}

static int FindInodeByPath(std::fstream& file, SuperBlock& sb, const std::string& path)
{
    std::vector<std::string> parts = SplitPath(path);
    if(parts.empty()) return 0;
    int blockSize = sizeof(FileBlock), cur = 0;
    for(const std::string& part : parts){
        Inode inode = BlockManager::ReadInode(file, sb, cur);
        if(inode.i_type != 0) return -1;
        bool found = false;
        for(int b = 0; b < 12 && !found; b++){
            if(inode.i_block[b] == -1) break;
            FolderBlock fb{};
            file.seekg(sb.s_block_start + (inode.i_block[b] * blockSize));
            file.read(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));
            for(int e = 0; e < 4 && !found; e++){
                if(fb.b_content[e].b_inodo == -1) continue;
                if(fb.b_content[e].b_name[0] == '\0') continue;
                std::string name(fb.b_content[e].b_name);
                if(name == "." || name == "..") continue;
                if(name == part){ cur = fb.b_content[e].b_inodo; found = true; }
            }
        }
        if(!found) return -1;
    }
    return cur;
}

// ======================================================
// ================= REPORTE MBR ========================
// ======================================================

static void RepMbr(const std::string& outPath, std::fstream& file)
{
    CreateDirs(outPath);

    MBR mbr{};
    file.seekg(0);
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

    std::string dotFile = outPath + ".dot";
    std::ofstream dot(dotFile);
    if(!dot.is_open()){ std::cout << "Error: No se pudo crear el archivo dot\n"; return; }

    // Colores
    const std::string HDR_MBR  = "#4a235a";
    const std::string HDR_PRIM = "#4a235a";
    const std::string HDR_LOG  = "#c0392b";
    const std::string ROW1     = "#ffffff";
    const std::string ROW2     = "#e8d5f0";
    const std::string LOG1     = "#ffffff";
    const std::string LOG2     = "#f5b7b1";

    // Helper filas
    auto row = [&](std::ofstream& d, const std::string& key, const std::string& val,
                   bool even, const std::string& c1, const std::string& c2){
        std::string bg = even ? c2 : c1;
        d << "    <TR>"
          << "<TD BGCOLOR=\"" << bg << "\" ALIGN=\"RIGHT\"><FONT COLOR=\"#333333\">" << key << "</FONT></TD>"
          << "<TD BGCOLOR=\"" << bg << "\" ALIGN=\"LEFT\"><FONT COLOR=\"#333333\">"  << val << "</FONT></TD>"
          << "</TR>\n";
    };

    auto header = [&](std::ofstream& d, const std::string& color, const std::string& label){
        d << "    <TR><TD COLSPAN=\"2\" BGCOLOR=\"" << color << "\" ALIGN=\"LEFT\">"
          << "<FONT COLOR=\"white\"><B>" << label << "</B></FONT></TD></TR>\n";
    };

    // ── Verificar si hay EBR reales ──
    bool hasEBR = false;
    for(int i = 0; i < 4; i++){
        if(mbr.mbr_partitions[i].part_s > 0 &&
           (mbr.mbr_partitions[i].part_type == 'e' || mbr.mbr_partitions[i].part_type == 'E')){
            // Ver si tiene EBRs dentro
            int ebrPos = mbr.mbr_partitions[i].part_start;
            EBR ebr{};
            file.seekg(ebrPos);
            file.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));
            if(ebr.part_s > 0) hasEBR = true;
            break;
        }
    }

    dot << "digraph G {\n";
    dot << "  graph [bgcolor=white rankdir=TB]\n";
    dot << "  node [shape=none margin=0]\n\n";

    // ── Nodo MBR ──
    dot << "  mbr_node [label=<\n";
    dot << "  <TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"6\" "
        << "BGCOLOR=\"white\" COLOR=\"#cccccc\" WIDTH=\"300\">\n";

    header(dot, HDR_MBR, "REPORTE DE MBR");
    row(dot, "mbr_tamano",         std::to_string(mbr.mbr_tamano),        false, ROW1, ROW2);
    row(dot, "mbr_fecha_creacion", TimeToStr(mbr.mbr_fecha_creacion),     true,  ROW1, ROW2);
    row(dot, "mbr_disk_signature", std::to_string(mbr.mbr_dsk_signature), false, ROW1, ROW2);

    // Particiones del MBR
    for(int i = 0; i < 4; i++){
        Partition& p = mbr.mbr_partitions[i];
        if(p.part_s <= 0) continue;

        header(dot, HDR_PRIM, "Particion");
        // row(dot, "part_status", std::string(1, p.part_status), false, ROW1, ROW2);
        bool isMounted = false;
        for(const auto& m : mountedList)
            if(m.name == std::string(p.part_name)) isMounted = true;
        row(dot, "part_status", isMounted ? "1" : std::string(1, p.part_status), false, ROW1, ROW2);
        row(dot, "part_type",   std::string(1, p.part_type),   true,  ROW1, ROW2);
        row(dot, "part_fit",    std::string(1, p.part_fit),    false, ROW1, ROW2);
        row(dot, "part_start",  std::to_string(p.part_start),  true,  ROW1, ROW2);
        row(dot, "part_size",   std::to_string(p.part_s),      false, ROW1, ROW2);
        row(dot, "part_name",   std::string(p.part_name),      true,  ROW1, ROW2);

        // Si es extendida, agregar sus EBR dentro del mismo nodo MBR
        if(p.part_type == 'e' || p.part_type == 'E'){
            int ebrPos = p.part_start;
            while(ebrPos != -1){
                EBR ebr{};
                file.seekg(ebrPos);
                file.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));
                if(ebr.part_s <= 0) break;

                header(dot, HDR_LOG, "Particion Logica");
                // row(dot, "part_status", std::string(1, ebr.part_mount), false, LOG1, LOG2);
                bool ebrMounted = false;
                for(const auto& m : mountedList)
                    if(m.name == std::string(ebr.part_name)) ebrMounted = true;
                row(dot, "part_status", ebrMounted ? "1" : std::string(1, ebr.part_mount), false, LOG1, LOG2);
                row(dot, "part_next",   std::to_string(ebr.part_next),  true,  LOG1, LOG2);
                row(dot, "part_fit",    std::string(1, ebr.part_fit),   false, LOG1, LOG2);
                row(dot, "part_start",  std::to_string(ebr.part_start), true,  LOG1, LOG2);
                row(dot, "part_size",   std::to_string(ebr.part_s),     false, LOG1, LOG2);
                row(dot, "part_name",   std::string(ebr.part_name),     true,  LOG1, LOG2);

                ebrPos = ebr.part_next;
            }
        }
    }

    dot << "  </TABLE>>]\n\n";

    // ── Nodo EBR separado (si hay lógicas) ──
    if(hasEBR){
        dot << "  ebr_node [label=<\n";
        dot << "  <TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"6\" "
            << "BGCOLOR=\"white\" COLOR=\"#cccccc\" WIDTH=\"300\">\n";

        // Título EBR sin fondo
        dot << "    <TR><TD COLSPAN=\"2\" BGCOLOR=\"white\" ALIGN=\"LEFT\">"
            << "<FONT COLOR=\"black\"><B>EBR</B></FONT></TD></TR>\n";

        for(int i = 0; i < 4; i++){
            Partition& p = mbr.mbr_partitions[i];
            if(p.part_s <= 0) continue;
            if(p.part_type != 'e' && p.part_type != 'E') continue;

            int ebrPos = p.part_start;
            while(ebrPos != -1){
                EBR ebr{};
                file.seekg(ebrPos);
                file.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));
                if(ebr.part_s <= 0) break;

                header(dot, HDR_PRIM, "Particion");
                // row(dot, "part_status", std::string(1, ebr.part_mount), false, ROW1, ROW2);
                bool ebrMounted2 = false;
                for(const auto& m : mountedList)
                    if(m.name == std::string(ebr.part_name)) ebrMounted2 = true;
                row(dot, "part_status", ebrMounted2 ? "1" : std::string(1, ebr.part_mount), false, ROW1, ROW2);
                row(dot, "part_type",   "p",                            true,  ROW1, ROW2);
                row(dot, "part_fit",    std::string(1, ebr.part_fit),   false, ROW1, ROW2);
                row(dot, "part_start",  std::to_string(ebr.part_start), true,  ROW1, ROW2);
                row(dot, "part_size",   std::to_string(ebr.part_s),     false, ROW1, ROW2);
                row(dot, "part_name",   std::string(ebr.part_name),     true,  ROW1, ROW2);

                ebrPos = ebr.part_next;
            }
        }

        dot << "  </TABLE>>]\n\n";
        dot << "  mbr_node -> ebr_node [style=invis]\n";
    }

    dot << "}\n";
    dot.close();

    RunDot(dotFile, outPath);
    std::cout << "Reporte mbr generado: " << outPath << "\n";
}

// ======================================================
// ================= REP (dispatcher) ===================
// ======================================================

void Rep(const std::string& name,
         const std::string& path,
         const std::string& id,
         const std::string& pathFileLs)
{
    // Buscar partición montada
    MountedPartition* mounted = nullptr;
    for(auto& m : mountedList)
        if(m.id == id){ mounted = &m; break; }

    if(!mounted){ std::cout << "Error: ID no encontrado\n"; return; }

    auto file = FileUtils::OpenFile(mounted->path);
    if(!file.is_open()){ std::cout << "Error: No se pudo abrir el disco\n"; return; }

    if(name == "mbr"){
        RepMbr(path, file);
    } else {
        std::cout << "Reporte '" << name << "' aún no implementado\n";
    }

    file.close();
}

} // namespace RepManager