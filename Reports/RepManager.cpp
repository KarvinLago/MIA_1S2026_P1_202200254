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
#include <iomanip>
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
        row(dot, "part_status", std::string(1, p.part_status), false, ROW1, ROW2);
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
                row(dot, "part_status", std::string(1, ebr.part_mount), false, LOG1, LOG2);
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
                row(dot, "part_status", std::string(1, ebr.part_mount), false, ROW1, ROW2);
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
// ================= REPORTE DISK =======================
// ======================================================

static void RepDisk(const std::string& outPath, std::fstream& file,
                    const std::string& diskPath)
{
    CreateDirs(outPath);

    MBR mbr{};
    file.seekg(0);
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

    std::string dotFile = outPath + ".dot";
    std::ofstream dot(dotFile);
    if(!dot.is_open()){ std::cout << "Error: No se pudo crear archivo dot\n"; return; }

    int totalSize = mbr.mbr_tamano;

    // Nombre real del disco y tamaño
    std::string diskName = diskPath.substr(diskPath.find_last_of('/') + 1);
    double diskMB = (double)totalSize / (1024.0 * 1024.0);
    std::ostringstream ssMB;
    ssMB << std::fixed << std::setprecision(0) << diskMB << " MB";

    const std::string COLOR_MBR      = "#4a235a";
    const std::string COLOR_PRIMARY  = "#7d3c98";
    const std::string COLOR_EXTENDED = "#d5d8dc";
    const std::string COLOR_EBR      = "#a9cce3";
    const std::string COLOR_LOGICAL  = "#a9dfbf";
    const std::string COLOR_FREE     = "#f9e79f";
    const std::string FONT_DARK      = "#1a1a1a";

    auto pct = [&](int size) -> std::string {
        double p = (double)size / totalSize * 100.0;
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << p << "%";
        return ss.str();
    };

    // Recolectar particiones ordenadas por start
    std::vector<Partition> parts;
    for(int i = 0; i < 4; i++)
        if(mbr.mbr_partitions[i].part_s > 0)
            parts.push_back(mbr.mbr_partitions[i]);
    std::sort(parts.begin(), parts.end(),
              [](const Partition& a, const Partition& b){ return a.part_start < b.part_start; });

    struct Segment {
        std::string type; // "primary","extended","free"
        std::string name;
        int start, size;
    };

    std::vector<Segment> segments;
    int cursor = sizeof(MBR);

    for(const Partition& p : parts){
        if(p.part_start > cursor)
            segments.push_back({"free","Libre", cursor, p.part_start - cursor});
        if(p.part_type == 'e' || p.part_type == 'E')
            segments.push_back({"extended", std::string(p.part_name), p.part_start, p.part_s});
        else
            segments.push_back({"primary",  std::string(p.part_name), p.part_start, p.part_s});
        cursor = p.part_start + p.part_s;
    }
    if(cursor < totalSize)
        segments.push_back({"free","Libre", cursor, totalSize - cursor});

    const int TOTAL_WIDTH = 800;
    const int MBR_WIDTH   = 40;
    int usableWidth = TOTAL_WIDTH - MBR_WIDTH;
    int usableSize  = totalSize - (int)sizeof(MBR);

    dot << "digraph G {\n";
    dot << "  graph [bgcolor=white]\n";
    dot << "  node [shape=none margin=0]\n\n";
    dot << "  disk [label=<\n";
    dot << "  <TABLE BORDER=\"0\" CELLBORDER=\"0\" CELLSPACING=\"2\" CELLPADDING=\"0\">\n";

    // Título
    dot << "    <TR><TD COLSPAN=\"100\" ALIGN=\"CENTER\" CELLPADDING=\"8\">"
        << "<FONT COLOR=\"" << FONT_DARK << "\" POINT-SIZE=\"14\"><B>"
        << diskName << "</B><BR/>"
        << "<FONT POINT-SIZE=\"10\">" << ssMB.str() << "</FONT>"
        << "</FONT></TD></TR>\n";

    dot << "    <TR>\n";

    // MBR
    dot << "      <TD BGCOLOR=\"" << COLOR_MBR << "\" WIDTH=\"" << MBR_WIDTH << "\" HEIGHT=\"80\" "
        << "BORDER=\"1\" ALIGN=\"CENTER\" VALIGN=\"MIDDLE\" CELLPADDING=\"4\">"
        << "<FONT COLOR=\"white\"><B>MBR</B></FONT></TD>\n";

    for(const Segment& seg : segments){
        int w = std::max(35, (int)((double)seg.size / usableSize * usableWidth));

        if(seg.type == "free"){
            dot << "      <TD BGCOLOR=\"" << COLOR_FREE << "\" WIDTH=\"" << w << "\" HEIGHT=\"80\" "
                << "BORDER=\"1\" ALIGN=\"CENTER\" VALIGN=\"MIDDLE\" CELLPADDING=\"4\">"
                << "<FONT COLOR=\"" << FONT_DARK << "\"><B>Libre</B><BR/>"
                << "<FONT POINT-SIZE=\"9\">" << pct(seg.size) << " del disco</FONT></FONT></TD>\n";

        } else if(seg.type == "primary"){
            bool mounted = false;
            for(const auto& m : mountedList) if(m.name == seg.name) mounted = true;
            std::string color = mounted ? "#1a5276" : COLOR_PRIMARY;
            std::string extra = mounted ? "<BR/><FONT POINT-SIZE=\"8\">● Montada</FONT>" : "";
            dot << "      <TD BGCOLOR=\"" << color << "\" WIDTH=\"" << w << "\" HEIGHT=\"80\" "
                << "BORDER=\"" << (mounted ? "3" : "1") << "\" "
                << "COLOR=\"" << (mounted ? "#27ae60" : "#333333") << "\" "
                << "ALIGN=\"CENTER\" VALIGN=\"MIDDLE\" CELLPADDING=\"4\">"
                << "<FONT COLOR=\"white\"><B>" << seg.name << "</B><BR/>"
                << "<FONT POINT-SIZE=\"9\">" << pct(seg.size) << " del disco</FONT>"
                << extra << "</FONT></TD>\n";

        } else if(seg.type == "extended"){
            // Contenedor extendida con tabla anidada
            dot << "      <TD BGCOLOR=\"" << COLOR_EXTENDED << "\" WIDTH=\"" << w << "\" "
                << "BORDER=\"1\" CELLPADDING=\"3\" VALIGN=\"TOP\">\n";
            dot << "        <TABLE BORDER=\"0\" CELLBORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"0\">\n";

            // Título extendida
            dot << "          <TR><TD COLSPAN=\"100\" ALIGN=\"CENTER\" CELLPADDING=\"2\">"
                << "<FONT COLOR=\"" << FONT_DARK << "\" POINT-SIZE=\"10\"><B>"
                << seg.name << "</B></FONT></TD></TR>\n";
            dot << "          <TR>\n";

            int extEnd    = seg.start + seg.size;
            int extCursor = seg.start;
            int innerW    = w - 6;
            int ebrPos    = seg.start;

            while(ebrPos != -1 && ebrPos >= seg.start && ebrPos < extEnd){
                EBR ebr{};
                file.seekg(ebrPos);
                file.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));
                if(ebr.part_s <= 0) break;

                // EBR
                int ebrW = std::max(20, (int)((double)sizeof(EBR) / seg.size * innerW));
                dot << "          <TD BGCOLOR=\"" << COLOR_EBR << "\" WIDTH=\"" << ebrW << "\" HEIGHT=\"50\" "
                    << "BORDER=\"1\" ALIGN=\"CENTER\" VALIGN=\"MIDDLE\" CELLPADDING=\"2\">"
                    << "<FONT COLOR=\"" << FONT_DARK << "\" POINT-SIZE=\"9\"><B>EBR</B></FONT></TD>\n";

                // Lógica
                int logW = std::max(30, (int)((double)ebr.part_s / seg.size * innerW));
                bool logMounted = false;
                for(const auto& m : mountedList) if(m.name == std::string(ebr.part_name)) logMounted = true;
                std::string logColor = logMounted ? "#1e8449" : COLOR_LOGICAL;
                dot << "          <TD BGCOLOR=\"" << logColor << "\" WIDTH=\"" << logW << "\" HEIGHT=\"50\" "
                    << "BORDER=\"1\" ALIGN=\"CENTER\" VALIGN=\"MIDDLE\" CELLPADDING=\"2\">"
                    << "<FONT COLOR=\"" << FONT_DARK << "\" POINT-SIZE=\"9\"><B>"
                    << std::string(ebr.part_name) << "</B><BR/>"
                    << pct(ebr.part_s) << " del Disco</FONT></TD>\n";

                extCursor = ebr.part_start + ebr.part_s;
                ebrPos = ebr.part_next;
            }

            // Libre interno
            if(extCursor < extEnd){
                int freeSize = extEnd - extCursor;
                int freeW = std::max(20, (int)((double)freeSize / seg.size * innerW));
                dot << "          <TD BGCOLOR=\"#fdfefe\" WIDTH=\"" << freeW << "\" HEIGHT=\"50\" "
                    << "BORDER=\"1\" ALIGN=\"CENTER\" VALIGN=\"MIDDLE\" CELLPADDING=\"2\">"
                    << "<FONT COLOR=\"" << FONT_DARK << "\" POINT-SIZE=\"9\"><B>Libre</B><BR/>"
                    << pct(freeSize) << "</FONT></TD>\n";
            }

            dot << "          </TR>\n";
            dot << "        </TABLE>\n";
            dot << "      </TD>\n";
        }
    }

    dot << "    </TR>\n";
    dot << "  </TABLE>>]\n";
    dot << "}\n";
    dot.close();

    RunDot(dotFile, outPath);
    std::cout << "Reporte disk generado: " << outPath << "\n";
}

// ======================================================
// ================= REPORTE INODE ======================
// ======================================================

static void RepInode(const std::string& outPath, std::fstream& file)
{
    CreateDirs(outPath);

    // Leer SuperBlock
    SuperBlock sb{};
    // Necesitamos el partStart — lo leemos desde el archivo que ya está abierto en la partición
    // El caller ya posicionó; leemos desde inicio del archivo buscando el SB
    // En realidad el dispatcher pasa el file abierto desde el inicio del disco,
    // así que necesitamos leer el MBR para encontrar el start de la partición montada.
    // Usamos el mismo patrón que otros reportes: el file está abierto desde inicio del disco.

    std::string dotFile = outPath + ".dot";
    std::ofstream dot(dotFile);
    if(!dot.is_open()){ std::cout << "Error: No se pudo crear archivo dot\n"; return; }

    // Leer MBR para encontrar partStart (ya viene posicionado en el file del dispatcher)
    // El dispatcher reabre el disco desde el path montado, así que podemos leer MBR
    MBR mbr{};
    file.seekg(0);
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

    // Buscar partStart usando mountedList (tomamos el primer montado que coincida con este file)
    // El dispatcher ya filtró por ID, así que leemos el SB desde la partición activa
    // Para obtener partStart usamos FileUtils pattern: buscamos en MBR y EBR
    // Como no tenemos el nombre aquí, leemos todos los SB posibles buscando magic number
    int partStart = -1;
    const int EXT2_MAGIC = 0xEF53;

    // Buscar en primarias
    for(int i = 0; i < 4; i++){
        if(mbr.mbr_partitions[i].part_s > 0 &&
           mbr.mbr_partitions[i].part_type != 'e' &&
           mbr.mbr_partitions[i].part_type != 'E'){
            SuperBlock tmp{};
            file.seekg(mbr.mbr_partitions[i].part_start);
            file.read(reinterpret_cast<char*>(&tmp), sizeof(SuperBlock));
            if(tmp.s_magic == EXT2_MAGIC){ partStart = mbr.mbr_partitions[i].part_start; break; }
        }
    }
    // Buscar en lógicas si no encontró
    if(partStart == -1){
        for(int i = 0; i < 4; i++){
            if(mbr.mbr_partitions[i].part_s > 0 &&
               (mbr.mbr_partitions[i].part_type == 'e' || mbr.mbr_partitions[i].part_type == 'E')){
                int ebrPos = mbr.mbr_partitions[i].part_start;
                while(ebrPos != -1){
                    EBR ebr{};
                    file.seekg(ebrPos);
                    file.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));
                    if(ebr.part_s <= 0) break;
                    SuperBlock tmp{};
                    file.seekg(ebr.part_start);
                    file.read(reinterpret_cast<char*>(&tmp), sizeof(SuperBlock));
                    if(tmp.s_magic == EXT2_MAGIC){ partStart = ebr.part_start; break; }
                    ebrPos = ebr.part_next;
                }
                break;
            }
        }
    }

    if(partStart == -1){ std::cout << "Error: No se encontró partición EXT2\n"; dot.close(); return; }

    file.seekg(partStart);
    file.read(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));

    // Colores
    const std::string HDR   = "#1a5276";
    const std::string ROW1  = "#ffffff";
    const std::string ROW2  = "#d6eaf8";
    const std::string FONT  = "#1a1a1a";

    auto timeStr = [](time_t t) -> std::string {
        if(t <= 0) return "N/A";
        char buf[32];
        struct tm* tm_info = localtime(&t);
        strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M", tm_info);
        return std::string(buf);
    };

    auto row = [&](std::ofstream& d, const std::string& key, const std::string& val, bool even){
        std::string bg = even ? ROW2 : ROW1;
        d << "    <TR>"
          << "<TD BGCOLOR=\"" << bg << "\" ALIGN=\"RIGHT\" CELLPADDING=\"4\">"
          << "<FONT COLOR=\"" << FONT << "\">" << key << "</FONT></TD>"
          << "<TD BGCOLOR=\"" << bg << "\" ALIGN=\"LEFT\" CELLPADDING=\"4\">"
          << "<FONT COLOR=\"" << FONT << "\">" << val << "</FONT></TD>"
          << "</TR>\n";
    };

    dot << "digraph G {\n";
    dot << "  graph [bgcolor=white rankdir=LR]\n";
    dot << "  node [shape=none margin=0]\n\n";

    // Recorrer todos los inodos usados
    int totalInodes = sb.s_inodes_count;
    bool anyInode = false;
    std::string prevNode = "";

    for(int idx = 0; idx < totalInodes; idx++){
        // Leer bitmap de inodos
        file.seekg(sb.s_bm_inode_start + idx);
        char bm = 0;
        file.read(&bm, 1);
        if(bm != '1') continue; // no usado

        // Leer inodo
        Inode inode{};
        file.seekg(sb.s_inode_start + idx * sizeof(Inode));
        file.read(reinterpret_cast<char*>(&inode), sizeof(Inode));

        std::string nodeName = "inode_" + std::to_string(idx);
        anyInode = true;

        dot << "  " << nodeName << " [label=<\n";
        dot << "  <TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"4\" "
            << "BGCOLOR=\"white\" COLOR=\"#aaaaaa\" WIDTH=\"260\">\n";

        // Header
        dot << "    <TR><TD COLSPAN=\"2\" BGCOLOR=\"" << HDR << "\" ALIGN=\"CENTER\" CELLPADDING=\"6\">"
            << "<FONT COLOR=\"white\"><B>Inodo " << idx << "</B></FONT></TD></TR>\n";

        row(dot, "i_uid",   std::to_string(inode.i_uid),  false);
        row(dot, "i_gid",   std::to_string(inode.i_gid),  true);
        row(dot, "i_size",  std::to_string(inode.i_size), false);
        row(dot, "i_atime", timeStr(inode.i_atime),        true);
        row(dot, "i_ctime", timeStr(inode.i_ctime),        false);
        row(dot, "i_mtime", timeStr(inode.i_mtime),        true);
        // Sanitizar i_type: si es null o no imprimible, mostrar descripción
        std::string typeStr;
        if(inode.i_type == '0' || inode.i_type == 0)       typeStr = "0 (carpeta)";
        else if(inode.i_type == '1' || inode.i_type == 1)  typeStr = "1 (archivo)";
        else typeStr = std::string(1, inode.i_type);
        row(dot, "i_type",  typeStr,  false);
        row(dot, "i_perm",  std::to_string(inode.i_perm),  true);

        // Bloques usados
        for(int b = 0; b < 15; b++){
            if(inode.i_block[b] == -1) break;
            row(dot, "i_block[" + std::to_string(b) + "]",
                std::to_string(inode.i_block[b]), b % 2 == 0);
        }

        dot << "  </TABLE>>]\n\n";

        // Flecha al anterior
        if(!prevNode.empty())
            dot << "  " << prevNode << " -> " << nodeName << " [color=\"#2980b9\" penwidth=1.5]\n\n";

        prevNode = nodeName;
    }

    if(!anyInode)
        dot << "  empty [label=\"No hay inodos utilizados\" shape=box]\n";

    dot << "}\n";
    dot.close();

    RunDot(dotFile, outPath);
    std::cout << "Reporte inode generado: " << outPath << "\n";
}

// ======================================================
// ================= REPORTE BLOCK ======================
// ======================================================

static void RepBlock(const std::string& outPath, std::fstream& file)
{
    CreateDirs(outPath);

    MBR mbr{};
    file.seekg(0);
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

    // Encontrar partición EXT2
    int partStart = -1;
    const int EXT2_MAGIC = 0xEF53;
    for(int i = 0; i < 4 && partStart == -1; i++){
        if(mbr.mbr_partitions[i].part_s > 0 &&
           mbr.mbr_partitions[i].part_type != 'e' &&
           mbr.mbr_partitions[i].part_type != 'E'){
            SuperBlock tmp{};
            file.seekg(mbr.mbr_partitions[i].part_start);
            file.read(reinterpret_cast<char*>(&tmp), sizeof(SuperBlock));
            if(tmp.s_magic == EXT2_MAGIC) partStart = mbr.mbr_partitions[i].part_start;
        }
    }
    if(partStart == -1){
        for(int i = 0; i < 4 && partStart == -1; i++){
            if(mbr.mbr_partitions[i].part_s > 0 &&
               (mbr.mbr_partitions[i].part_type == 'e' || mbr.mbr_partitions[i].part_type == 'E')){
                int ebrPos = mbr.mbr_partitions[i].part_start;
                while(ebrPos != -1){
                    EBR ebr{};
                    file.seekg(ebrPos);
                    file.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));
                    if(ebr.part_s <= 0) break;
                    SuperBlock tmp{};
                    file.seekg(ebr.part_start);
                    file.read(reinterpret_cast<char*>(&tmp), sizeof(SuperBlock));
                    if(tmp.s_magic == EXT2_MAGIC){ partStart = ebr.part_start; break; }
                    ebrPos = ebr.part_next;
                }
            }
        }
    }
    if(partStart == -1){ std::cout << "Error: No se encontró partición EXT2\n"; return; }

    SuperBlock sb{};
    file.seekg(partStart);
    file.read(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));

    std::string dotFile = outPath + ".dot";
    std::ofstream dot(dotFile);
    if(!dot.is_open()){ std::cout << "Error: No se pudo crear archivo dot\n"; return; }

    const std::string HDR_FOLDER  = "#1e8449";  // verde carpeta
    const std::string HDR_FILE    = "#1a5276";  // azul archivo
    const std::string HDR_PTR     = "#7d3c98";  // morado apuntadores
    const std::string ROW1        = "#ffffff";
    const std::string ROW2_FOLDER = "#d5f5e3";
    const std::string ROW2_FILE   = "#d6eaf8";
    const std::string ROW2_PTR    = "#e8d5f0";
    const std::string FONT        = "#1a1a1a";

    dot << "digraph G {\n";
    dot << "  graph [bgcolor=white rankdir=LR]\n";
    dot << "  node [shape=none margin=0]\n\n";

    // Conjunto de bloques ya procesados para no repetir
    std::vector<bool> rendered(sb.s_blocks_count, false);

    bool anyBlock   = false;
    std::string prevNode = "";

    // Recorrer inodos usados
    for(int idx = 0; idx < sb.s_inodes_count; idx++){
        file.seekg(sb.s_bm_inode_start + idx);
        char bm = 0;
        file.read(&bm, 1);
        if(bm != '1') continue;

        Inode inode{};
        file.seekg(sb.s_inode_start + idx * sizeof(Inode));
        file.read(reinterpret_cast<char*>(&inode), sizeof(Inode));

        bool isFolder = (inode.i_type == 0);
        bool isFile   = (inode.i_type == 1);

        // Lambda para emitir nodo de bloque carpeta
        auto emitFolderBlock = [&](int blockIdx){
            if(blockIdx < 0 || blockIdx >= sb.s_blocks_count) return;
            if(rendered[blockIdx]) return;
            rendered[blockIdx] = true;
            anyBlock = true;

            FolderBlock fb{};
            file.seekg(sb.s_block_start + blockIdx * (int)sizeof(FileBlock));
            file.read(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));

            std::string nName = "block_" + std::to_string(blockIdx);
            dot << "  " << nName << " [label=<\n";
            dot << "  <TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"4\" "
                << "BGCOLOR=\"white\" COLOR=\"#aaaaaa\" WIDTH=\"220\">\n";
            dot << "    <TR><TD COLSPAN=\"2\" BGCOLOR=\"" << HDR_FOLDER
                << "\" ALIGN=\"CENTER\" CELLPADDING=\"6\">"
                << "<FONT COLOR=\"white\"><B>Bloque Carpeta " << blockIdx
                << "</B></FONT></TD></TR>\n";
            // Header columnas
            dot << "    <TR>"
                << "<TD BGCOLOR=\"" << ROW2_FOLDER << "\" ALIGN=\"CENTER\"><FONT COLOR=\"" << FONT << "\"><B>b_name</B></FONT></TD>"
                << "<TD BGCOLOR=\"" << ROW2_FOLDER << "\" ALIGN=\"CENTER\"><FONT COLOR=\"" << FONT << "\"><B>b_inodo</B></FONT></TD>"
                << "</TR>\n";
            for(int e = 0; e < 4; e++){
                std::string bg = (e % 2 == 0) ? ROW1 : ROW2_FOLDER;
                std::string bname = (fb.b_content[e].b_name[0] != '\0')
                                    ? std::string(fb.b_content[e].b_name) : "-";
                std::string binodo = (fb.b_content[e].b_inodo != -1)
                                     ? std::to_string(fb.b_content[e].b_inodo) : "-1";
                dot << "    <TR>"
                    << "<TD BGCOLOR=\"" << bg << "\" ALIGN=\"LEFT\" CELLPADDING=\"3\">"
                    << "<FONT COLOR=\"" << FONT << "\">" << bname << "</FONT></TD>"
                    << "<TD BGCOLOR=\"" << bg << "\" ALIGN=\"RIGHT\" CELLPADDING=\"3\">"
                    << "<FONT COLOR=\"" << FONT << "\">" << binodo << "</FONT></TD>"
                    << "</TR>\n";
            }
            dot << "  </TABLE>>]\n\n";

            if(!prevNode.empty())
                dot << "  " << prevNode << " -> " << nName << " [color=\"#27ae60\" penwidth=1.5]\n\n";
            prevNode = nName;
        };

        // Lambda para emitir nodo de bloque archivo
        auto emitFileBlock = [&](int blockIdx){
            if(blockIdx < 0 || blockIdx >= sb.s_blocks_count) return;
            if(rendered[blockIdx]) return;
            rendered[blockIdx] = true;
            anyBlock = true;

            FileBlock fb{};
            file.seekg(sb.s_block_start + blockIdx * (int)sizeof(FileBlock));
            file.read(reinterpret_cast<char*>(&fb), sizeof(FileBlock));

            // Sanitizar contenido para HTML
            std::string content;
            for(int c = 0; c < (int)sizeof(fb.b_content); c++){
                char ch = fb.b_content[c];
                if(ch == '\0') break;
                if(ch == '<') content += "&lt;";
                else if(ch == '>') content += "&gt;";
                else if(ch == '&') content += "&amp;";
                else if(ch == '"') content += "&quot;";
                else content += ch;
            }
            if(content.empty()) content = "(vacío)";

            std::string nName = "block_" + std::to_string(blockIdx);
            dot << "  " << nName << " [label=<\n";
            dot << "  <TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"4\" "
                << "BGCOLOR=\"white\" COLOR=\"#aaaaaa\" WIDTH=\"220\">\n";
            dot << "    <TR><TD BGCOLOR=\"" << HDR_FILE
                << "\" ALIGN=\"CENTER\" CELLPADDING=\"6\">"
                << "<FONT COLOR=\"white\"><B>Bloque Archivo " << blockIdx
                << "</B></FONT></TD></TR>\n";
            dot << "    <TR><TD BGCOLOR=\"" << ROW2_FILE
                << "\" ALIGN=\"LEFT\" CELLPADDING=\"4\">"
                << "<FONT COLOR=\"" << FONT << "\" POINT-SIZE=\"9\">" << content
                << "</FONT></TD></TR>\n";
            dot << "  </TABLE>>]\n\n";

            if(!prevNode.empty())
                dot << "  " << prevNode << " -> " << nName << " [color=\"#2980b9\" penwidth=1.5]\n\n";
            prevNode = nName;
        };

        // Lambda para emitir nodo de bloque apuntadores
        auto emitPtrBlock = [&](int blockIdx){
            if(blockIdx < 0 || blockIdx >= sb.s_blocks_count) return;
            if(rendered[blockIdx]) return;
            rendered[blockIdx] = true;
            anyBlock = true;

            PointerBlock pb{};
            file.seekg(sb.s_block_start + blockIdx * (int)sizeof(FileBlock));
            file.read(reinterpret_cast<char*>(&pb), sizeof(PointerBlock));

            std::string nName = "block_" + std::to_string(blockIdx);
            dot << "  " << nName << " [label=<\n";
            dot << "  <TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"4\" "
                << "BGCOLOR=\"white\" COLOR=\"#aaaaaa\" WIDTH=\"220\">\n";
            dot << "    <TR><TD BGCOLOR=\"" << HDR_PTR
                << "\" ALIGN=\"CENTER\" CELLPADDING=\"6\">"
                << "<FONT COLOR=\"white\"><B>Bloque Apuntadores " << blockIdx
                << "</B></FONT></TD></TR>\n";

            std::string ptrs;
            for(int p = 0; p < 16; p++){
                ptrs += std::to_string(pb.b_pointers[p]);
                if(p < 15) ptrs += ", ";
            }
            dot << "    <TR><TD BGCOLOR=\"" << ROW2_PTR
                << "\" ALIGN=\"LEFT\" CELLPADDING=\"4\">"
                << "<FONT COLOR=\"" << FONT << "\" POINT-SIZE=\"9\">" << ptrs
                << "</FONT></TD></TR>\n";
            dot << "  </TABLE>>]\n\n";

            if(!prevNode.empty())
                dot << "  " << prevNode << " -> " << nName << " [color=\"#8e44ad\" penwidth=1.5]\n\n";
            prevNode = nName;
        };

        if(isFolder){
            for(int b = 0; b < 12; b++){
                if(inode.i_block[b] == -1) break;
                emitFolderBlock(inode.i_block[b]);
            }
        } else if(isFile){
            // Directos
            for(int b = 0; b < 12; b++){
                if(inode.i_block[b] == -1) break;
                emitFileBlock(inode.i_block[b]);
            }
            // Simple indirecto
            if(inode.i_block[12] != -1){
                emitPtrBlock(inode.i_block[12]);
                PointerBlock pb{};
                file.seekg(sb.s_block_start + inode.i_block[12] * (int)sizeof(FileBlock));
                file.read(reinterpret_cast<char*>(&pb), sizeof(PointerBlock));
                for(int p = 0; p < 16; p++){
                    if(pb.b_pointers[p] == -1) break;
                    emitFileBlock(pb.b_pointers[p]);
                }
            }
            // Doble indirecto
            if(inode.i_block[13] != -1){
                emitPtrBlock(inode.i_block[13]);
                PointerBlock l1{};
                file.seekg(sb.s_block_start + inode.i_block[13] * (int)sizeof(FileBlock));
                file.read(reinterpret_cast<char*>(&l1), sizeof(PointerBlock));
                for(int i = 0; i < 16; i++){
                    if(l1.b_pointers[i] == -1) break;
                    emitPtrBlock(l1.b_pointers[i]);
                    PointerBlock l2{};
                    file.seekg(sb.s_block_start + l1.b_pointers[i] * (int)sizeof(FileBlock));
                    file.read(reinterpret_cast<char*>(&l2), sizeof(PointerBlock));
                    for(int j = 0; j < 16; j++){
                        if(l2.b_pointers[j] == -1) break;
                        emitFileBlock(l2.b_pointers[j]);
                    }
                }
            }
            // Triple indirecto
            if(inode.i_block[14] != -1){
                emitPtrBlock(inode.i_block[14]);
                PointerBlock l1{};
                file.seekg(sb.s_block_start + inode.i_block[14] * (int)sizeof(FileBlock));
                file.read(reinterpret_cast<char*>(&l1), sizeof(PointerBlock));
                for(int i = 0; i < 16; i++){
                    if(l1.b_pointers[i] == -1) break;
                    emitPtrBlock(l1.b_pointers[i]);
                    PointerBlock l2{};
                    file.seekg(sb.s_block_start + l1.b_pointers[i] * (int)sizeof(FileBlock));
                    file.read(reinterpret_cast<char*>(&l2), sizeof(PointerBlock));
                    for(int j = 0; j < 16; j++){
                        if(l2.b_pointers[j] == -1) break;
                        emitPtrBlock(l2.b_pointers[j]);
                        PointerBlock l3{};
                        file.seekg(sb.s_block_start + l2.b_pointers[j] * (int)sizeof(FileBlock));
                        file.read(reinterpret_cast<char*>(&l3), sizeof(PointerBlock));
                        for(int k = 0; k < 16; k++){
                            if(l3.b_pointers[k] == -1) break;
                            emitFileBlock(l3.b_pointers[k]);
                        }
                    }
                }
            }
        }
    }

    if(!anyBlock)
        dot << "  empty [label=\"No hay bloques utilizados\" shape=box]\n";

    dot << "}\n";
    dot.close();

    RunDot(dotFile, outPath);
    std::cout << "Reporte block generado: " << outPath << "\n";
}

// ======================================================
// ================= REP (dispatcher) ===================
// ======================================================

// ======================================================
// ======= HELPER: encontrar partición EXT2 =============
// ======================================================

static int FindEXT2Partition(std::fstream& file)
{
    MBR mbr{};
    file.seekg(0);
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

    const int EXT2_MAGIC = 0xEF53;
    // Buscar en primarias
    for(int i = 0; i < 4; i++){
        if(mbr.mbr_partitions[i].part_s > 0 &&
           mbr.mbr_partitions[i].part_type != 'e' &&
           mbr.mbr_partitions[i].part_type != 'E'){
            SuperBlock tmp{};
            file.seekg(mbr.mbr_partitions[i].part_start);
            file.read(reinterpret_cast<char*>(&tmp), sizeof(SuperBlock));
            if(tmp.s_magic == EXT2_MAGIC) return mbr.mbr_partitions[i].part_start;
        }
    }
    // Buscar en lógicas
    for(int i = 0; i < 4; i++){
        if(mbr.mbr_partitions[i].part_s > 0 &&
           (mbr.mbr_partitions[i].part_type == 'e' || mbr.mbr_partitions[i].part_type == 'E')){
            int ebrPos = mbr.mbr_partitions[i].part_start;
            while(ebrPos != -1){
                EBR ebr{};
                file.seekg(ebrPos);
                file.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));
                if(ebr.part_s <= 0) break;
                SuperBlock tmp{};
                file.seekg(ebr.part_start);
                file.read(reinterpret_cast<char*>(&tmp), sizeof(SuperBlock));
                if(tmp.s_magic == EXT2_MAGIC) return ebr.part_start;
                ebrPos = ebr.part_next;
            }
        }
    }
    return -1;
}

// ======================================================
// ================= REPORTE BM_INODE ===================
// ======================================================

static void RepBmInode(const std::string& outPath, std::fstream& file)
{
    CreateDirs(outPath);

    int partStart = FindEXT2Partition(file);
    if(partStart == -1){ std::cout << "Error: No se encontró partición EXT2\n"; return; }

    SuperBlock sb{};
    file.seekg(partStart);
    file.read(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));

    // Leer bitmap completo de inodos
    std::vector<char> bitmap(sb.s_inodes_count);
    file.seekg(sb.s_bm_inode_start);
    file.read(bitmap.data(), sb.s_inodes_count);

    std::ofstream out(outPath);
    if(!out.is_open()){ std::cout << "Error: No se pudo crear archivo\n"; return; }

    for(int i = 0; i < sb.s_inodes_count; i++){
        out << bitmap[i]; // '0' o '1'
        if((i + 1) % 20 == 0)
            out << "\n";
        else if(i + 1 < sb.s_inodes_count)
            out << " ";
    }
    out << "\n";
    out.close();

    std::cout << "Reporte bm_inode generado: " << outPath << "\n";
}

// ======================================================
// ================= REPORTE BM_BLOCK ===================
// ======================================================

static void RepBmBlock(const std::string& outPath, std::fstream& file)
{
    CreateDirs(outPath);

    int partStart = FindEXT2Partition(file);
    if(partStart == -1){ std::cout << "Error: No se encontró partición EXT2\n"; return; }

    SuperBlock sb{};
    file.seekg(partStart);
    file.read(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));

    // Leer bitmap completo de bloques
    std::vector<char> bitmap(sb.s_blocks_count);
    file.seekg(sb.s_bm_block_start);
    file.read(bitmap.data(), sb.s_blocks_count);

    std::ofstream out(outPath);
    if(!out.is_open()){ std::cout << "Error: No se pudo crear archivo\n"; return; }

    for(int i = 0; i < sb.s_blocks_count; i++){
        out << bitmap[i]; // '0' o '1'
        if((i + 1) % 20 == 0)
            out << "\n";
        else if(i + 1 < sb.s_blocks_count)
            out << " ";
    }
    out << "\n";
    out.close();

    std::cout << "Reporte bm_block generado: " << outPath << "\n";
}

// ======================================================
// ================= REPORTE TREE =======================
// ======================================================

static void RepTreeVisit(std::ofstream& dot, std::fstream& file, SuperBlock& sb,
                         int inodeIdx, const std::string& inodePath,
                         std::vector<bool>& visitedInodes);

static void RepTreeEmitBlock(std::ofstream& dot, std::fstream& file, SuperBlock& sb,
                             int blockIdx, bool isFolder,
                             std::vector<bool>& visitedInodes,
                             const std::string& parentNode)
{
    if(blockIdx < 0 || blockIdx >= sb.s_blocks_count) return;
    std::string bNode = "tb_" + std::to_string(blockIdx);

    if(isFolder){
        FolderBlock fb{};
        file.seekg(sb.s_block_start + blockIdx * (int)sizeof(FileBlock));
        file.read(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));

        dot << "  " << bNode << " [label=<\n";
        dot << "  <TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"3\" "
            << "BGCOLOR=\"#f1948a\" COLOR=\"#922b21\" WIDTH=\"160\">\n";
        dot << "    <TR><TD COLSPAN=\"2\" BGCOLOR=\"#e74c3c\" ALIGN=\"CENTER\" CELLPADDING=\"4\">"
            << "<FONT COLOR=\"white\" POINT-SIZE=\"9\"><B>b. carpetas " << blockIdx
            << "</B></FONT></TD></TR>\n";
        for(int e = 0; e < 4; e++){
            std::string bg = (e%2==0) ? "#ffffff" : "#f9ebea";
            std::string bn = (fb.b_content[e].b_name[0] != '\0')
                             ? std::string(fb.b_content[e].b_name) : "-";
            std::string bi = (fb.b_content[e].b_inodo != -1)
                             ? std::to_string(fb.b_content[e].b_inodo) : "-1";
            dot << "    <TR>"
                << "<TD BGCOLOR=\"" << bg << "\" ALIGN=\"LEFT\" CELLPADDING=\"2\" PORT=\"e" << e << "\">"
                << "<FONT POINT-SIZE=\"8\">" << bn << "</FONT></TD>"
                << "<TD BGCOLOR=\"" << bg << "\" ALIGN=\"RIGHT\" CELLPADDING=\"2\">"
                << "<FONT POINT-SIZE=\"8\">" << bi << "</FONT></TD>"
                << "</TR>\n";
        }
        dot << "  </TABLE>>]\n\n";
        dot << "  " << parentNode << " -> " << bNode
            << " [color=\"#e74c3c\" penwidth=1.2]\n\n";

        // Recursión a hijos no especiales
        for(int e = 0; e < 4; e++){
            int childIdx = fb.b_content[e].b_inodo;
            if(childIdx == -1) continue;
            std::string cn(fb.b_content[e].b_name);
            if(cn == "." || cn == "..") continue;
            if(childIdx < sb.s_inodes_count && !visitedInodes[childIdx]){
                std::string childPath = cn;
                RepTreeVisit(dot, file, sb, childIdx, childPath, visitedInodes);
                dot << "  " << bNode << ":e" << e << " -> ti_" << childIdx
                    << " [color=\"#2980b9\" penwidth=1.0 style=dashed]\n\n";
            }
        }
    } else {
        // Bloque archivo
        FileBlock fb{};
        file.seekg(sb.s_block_start + blockIdx * (int)sizeof(FileBlock));
        file.read(reinterpret_cast<char*>(&fb), sizeof(FileBlock));

        std::string content;
        for(int c = 0; c < (int)sizeof(fb.b_content); c++){
            char ch = fb.b_content[c];
            if(ch == '\0') break;
            if(ch == '<') content += "&lt;";
            else if(ch == '>') content += "&gt;";
            else if(ch == '&') content += "&amp;";
            else content += ch;
        }
        if(content.empty()) content = "(vacío)";

        dot << "  " << bNode << " [label=<\n";
        dot << "  <TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"3\" "
            << "BGCOLOR=\"#fef9e7\" COLOR=\"#d4ac0d\" WIDTH=\"160\">\n";
        dot << "    <TR><TD BGCOLOR=\"#f7dc6f\" ALIGN=\"CENTER\" CELLPADDING=\"4\">"
            << "<FONT COLOR=\"#7d6608\" POINT-SIZE=\"9\"><B>b. archivos " << blockIdx
            << "</B></FONT></TD></TR>\n";
        dot << "    <TR><TD BGCOLOR=\"#fef9e7\" ALIGN=\"LEFT\" CELLPADDING=\"3\">"
            << "<FONT POINT-SIZE=\"8\">" << content << "</FONT></TD></TR>\n";
        dot << "  </TABLE>>]\n\n";
        dot << "  " << parentNode << " -> " << bNode
            << " [color=\"#d4ac0d\" penwidth=1.2]\n\n";
    }
}

static void RepTreeVisit(std::ofstream& dot, std::fstream& file, SuperBlock& sb,
                         int inodeIdx, const std::string& inodePath,
                         std::vector<bool>& visitedInodes)
{
    if(inodeIdx < 0 || inodeIdx >= sb.s_inodes_count) return;
    if(visitedInodes[inodeIdx]) return;
    visitedInodes[inodeIdx] = true;

    Inode inode{};
    file.seekg(sb.s_inode_start + inodeIdx * sizeof(Inode));
    file.read(reinterpret_cast<char*>(&inode), sizeof(Inode));

    bool isFolder = (inode.i_type == 0);
    bool isFile   = (inode.i_type == 1);

    std::string iNode = "ti_" + std::to_string(inodeIdx);

    // Etiqueta de ruta encima
    dot << "  lbl_" << inodeIdx << " [label=\"" << inodePath
        << "\" shape=none fontsize=9 fontcolor=\"#1a1a1a\"]\n";
    dot << "  lbl_" << inodeIdx << " -> " << iNode << " [style=invis]\n\n";

    // Nodo inodo
    std::string typeStr = isFolder ? "0 (carpeta)" : (isFile ? "1 (archivo)" : "?");
    int ap0 = inode.i_block[0];
    int ap1 = inode.i_block[12];
    int ap2 = inode.i_block[13];

    dot << "  " << iNode << " [label=<\n";
    dot << "  <TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"3\" "
        << "BGCOLOR=\"#aed6f1\" COLOR=\"#1a5276\" WIDTH=\"140\">\n";
    dot << "    <TR><TD COLSPAN=\"2\" BGCOLOR=\"#2980b9\" ALIGN=\"CENTER\" CELLPADDING=\"4\">"
        << "<FONT COLOR=\"white\" POINT-SIZE=\"9\"><B>inodo " << inodeIdx
        << "</B></FONT></TD></TR>\n";

    auto irow = [&](const std::string& k, const std::string& v, bool even, const std::string& port=""){
        std::string bg = even ? "#d6eaf8" : "#ffffff";
        std::string portAttr = port.empty() ? "" : " PORT=\"" + port + "\"";
        dot << "    <TR>"
            << "<TD BGCOLOR=\"" << bg << "\" ALIGN=\"LEFT\" CELLPADDING=\"2\">"
            << "<FONT POINT-SIZE=\"8\">" << k << "</FONT></TD>"
            << "<TD BGCOLOR=\"" << bg << "\" ALIGN=\"RIGHT\" CELLPADDING=\"2\"" << portAttr << ">"
            << "<FONT POINT-SIZE=\"8\">" << v << "</FONT></TD>"
            << "</TR>\n";
    };

    irow("i_TYPE", typeStr,       false);
    irow("ap0",    std::to_string(ap0), true,  "ap0");
    irow("ap1",    ap1 != -1 ? std::to_string(ap1) : "-1", false, "ap1");
    irow("ap2",    ap2 != -1 ? std::to_string(ap2) : "-1", true,  "ap2");
    irow("i_perm", std::to_string(inode.i_perm), false);

    dot << "  </TABLE>>]\n\n";

    // Flecha ap0 → bloque directo
    if(ap0 != -1){
        RepTreeEmitBlock(dot, file, sb, ap0, isFolder, visitedInodes, iNode + ":ap0");
    }

    // ap1 → bloque apuntador simple
    if(ap1 != -1){
        std::string pNode = "tb_" + std::to_string(ap1);
        PointerBlock pb{};
        file.seekg(sb.s_block_start + ap1 * (int)sizeof(FileBlock));
        file.read(reinterpret_cast<char*>(&pb), sizeof(PointerBlock));

        // Emitir bloque apuntador
        std::string ptrs;
        for(int p = 0; p < 16; p++){
            ptrs += std::to_string(pb.b_pointers[p]);
            if(p < 15) ptrs += ", ";
        }
        dot << "  " << pNode << " [label=<\n";
        dot << "  <TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"3\" "
            << "BGCOLOR=\"#f5cba7\" COLOR=\"#922b21\" WIDTH=\"160\">\n";
        dot << "    <TR><TD BGCOLOR=\"#e59866\" ALIGN=\"CENTER\" CELLPADDING=\"4\">"
            << "<FONT COLOR=\"white\" POINT-SIZE=\"9\"><B>b. apuntadores " << ap1
            << "</B></FONT></TD></TR>\n";
        dot << "    <TR><TD BGCOLOR=\"#fef5ec\" ALIGN=\"LEFT\" CELLPADDING=\"3\">"
            << "<FONT POINT-SIZE=\"8\">" << ptrs << "</FONT></TD></TR>\n";
        dot << "  </TABLE>>]\n\n";
        dot << "  " << iNode << ":ap1 -> " << pNode << " [color=\"#e59866\" penwidth=1.2]\n\n";

        for(int p = 0; p < 16; p++){
            if(pb.b_pointers[p] == -1) break;
            RepTreeEmitBlock(dot, file, sb, pb.b_pointers[p], isFolder, visitedInodes, pNode);
        }
    }

    // ap2 → bloque apuntador doble
    if(ap2 != -1){
        std::string p1Node = "tb_" + std::to_string(ap2);
        PointerBlock l1{};
        file.seekg(sb.s_block_start + ap2 * (int)sizeof(FileBlock));
        file.read(reinterpret_cast<char*>(&l1), sizeof(PointerBlock));

        std::string ptrs1;
        for(int p = 0; p < 16; p++){
            ptrs1 += std::to_string(l1.b_pointers[p]);
            if(p < 15) ptrs1 += ", ";
        }
        dot << "  " << p1Node << " [label=<\n";
        dot << "  <TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"3\" "
            << "BGCOLOR=\"#f5cba7\" COLOR=\"#922b21\" WIDTH=\"160\">\n";
        dot << "    <TR><TD BGCOLOR=\"#e59866\" ALIGN=\"CENTER\" CELLPADDING=\"4\">"
            << "<FONT COLOR=\"white\" POINT-SIZE=\"9\"><B>b. apuntadores " << ap2
            << "</B></FONT></TD></TR>\n";
        dot << "    <TR><TD BGCOLOR=\"#fef5ec\" ALIGN=\"LEFT\" CELLPADDING=\"3\">"
            << "<FONT POINT-SIZE=\"8\">" << ptrs1 << "</FONT></TD></TR>\n";
        dot << "  </TABLE>>]\n\n";
        dot << "  " << iNode << ":ap2 -> " << p1Node << " [color=\"#e59866\" penwidth=1.2]\n\n";

        for(int i = 0; i < 16; i++){
            if(l1.b_pointers[i] == -1) break;
            std::string p2Node = "tb_" + std::to_string(l1.b_pointers[i]);
            PointerBlock l2{};
            file.seekg(sb.s_block_start + l1.b_pointers[i] * (int)sizeof(FileBlock));
            file.read(reinterpret_cast<char*>(&l2), sizeof(PointerBlock));

            std::string ptrs2;
            for(int p = 0; p < 16; p++){
                ptrs2 += std::to_string(l2.b_pointers[p]);
                if(p < 15) ptrs2 += ", ";
            }
            dot << "  " << p2Node << " [label=<\n";
            dot << "  <TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"3\" "
                << "BGCOLOR=\"#f5cba7\" COLOR=\"#922b21\" WIDTH=\"160\">\n";
            dot << "    <TR><TD BGCOLOR=\"#e59866\" ALIGN=\"CENTER\" CELLPADDING=\"4\">"
                << "<FONT COLOR=\"white\" POINT-SIZE=\"9\"><B>b. apuntadores " << l1.b_pointers[i]
                << "</B></FONT></TD></TR>\n";
            dot << "    <TR><TD BGCOLOR=\"#fef5ec\" ALIGN=\"LEFT\" CELLPADDING=\"3\">"
                << "<FONT POINT-SIZE=\"8\">" << ptrs2 << "</FONT></TD></TR>\n";
            dot << "  </TABLE>>]\n\n";
            dot << "  " << p1Node << " -> " << p2Node << " [color=\"#e59866\" penwidth=1.2]\n\n";

            for(int j = 0; j < 16; j++){
                if(l2.b_pointers[j] == -1) break;
                RepTreeEmitBlock(dot, file, sb, l2.b_pointers[j], isFolder, visitedInodes, p2Node);
            }
        }
    }
}

static void RepTree(const std::string& outPath, std::fstream& file)
{
    CreateDirs(outPath);

    int partStart = FindEXT2Partition(file);
    if(partStart == -1){ std::cout << "Error: No se encontró partición EXT2\n"; return; }

    SuperBlock sb{};
    file.seekg(partStart);
    file.read(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));

    std::string dotFile = outPath + ".dot";
    std::ofstream dot(dotFile);
    if(!dot.is_open()){ std::cout << "Error: No se pudo crear archivo dot\n"; return; }

    dot << "digraph G {\n";
    dot << "  graph [bgcolor=white rankdir=LR splines=ortho nodesep=0.5 ranksep=1.0]\n";
    dot << "  node [shape=none margin=0]\n\n";

    std::vector<bool> visitedInodes(sb.s_inodes_count, false);
    RepTreeVisit(dot, file, sb, 0, "/", visitedInodes);

    dot << "}\n";
    dot.close();

    RunDot(dotFile, outPath);
    std::cout << "Reporte tree generado: " << outPath << "\n";
}

// ======================================================
// ================= REP (dispatcher) ===================
// ======================================================

void Rep(const std::string& name,
         const std::string& path,
         const std::string& id,
         const std::string& pathFileLs)
{
    MountedPartition* mounted = nullptr;
    for(auto& m : mountedList)
        if(m.id == id){ mounted = &m; break; }

    if(!mounted){ std::cout << "Error: ID no encontrado\n"; return; }

    auto file = FileUtils::OpenFile(mounted->path);
    if(!file.is_open()){ std::cout << "Error: No se pudo abrir el disco\n"; return; }

    if(name == "mbr"){
        RepMbr(path, file);
    } else if(name == "disk"){
        RepDisk(path, file, mounted->path);
    } else if(name == "inode"){
        RepInode(path, file);
    } else if(name == "block"){
        RepBlock(path, file);
    } else if(name == "bm_inode"){
        RepBmInode(path, file);
    } else if(name == "bm_block" || name == "bm_bloc"){
        RepBmBlock(path, file);
    } else if(name == "tree"){
        RepTree(path, file);
    } else {
        std::cout << "Reporte '" << name << "' aún no implementado\n";
    }

    file.close();
}

} // namespace RepManager