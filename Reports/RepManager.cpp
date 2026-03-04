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
#include <iomanip>
#include <sys/stat.h>
#include <algorithm>

extern std::vector<MountedPartition> mountedList;

namespace RepManager {

// ======================================================
// ================= UTILIDADES =========================
// ======================================================

// Crear carpetas padre si no existen
static void CreateDirs(const std::string& path)
{
    std::string dir = path.substr(0, path.find_last_of('/'));
    if(dir.empty()) return;
    std::string cmd = "mkdir -p \"" + dir + "\"";
    system(cmd.c_str());
}

// Ejecutar dot para generar imagen
static void RunDot(const std::string& dotFile, const std::string& outFile)
{
    std::string ext = outFile.substr(outFile.find_last_of('.') + 1);
    std::string fmt = ext;
    if(fmt == "jpg" || fmt == "jpeg") fmt = "jpg";
    else if(fmt == "png") fmt = "png";
    else if(fmt == "pdf") fmt = "pdf";
    else fmt = "jpg";

    std::string cmd = "dot -T" + fmt + " \"" + dotFile + "\" -o \"" + outFile + "\"";
    system(cmd.c_str());
}

// Tiempo a string
static std::string TimeToStr(time_t t)
{
    if(t == 0) return "N/A";
    char buf[64];
    struct tm* tm_info = localtime(&t);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm_info);
    return std::string(buf);
}

// Split path
static std::vector<std::string> SplitPath(const std::string& path)
{
    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string token;
    while(std::getline(ss, token, '/'))
        if(!token.empty()) parts.push_back(token);
    return parts;
}

// Encontrar inodo por ruta
static int FindInodeByPath(std::fstream& file, SuperBlock& sb,
                           const std::string& path)
{
    std::vector<std::string> parts = SplitPath(path);
    if(parts.empty()) return 0;

    int blockSize = sizeof(FileBlock);
    int currentInodeIndex = 0;

    for(const std::string& part : parts)
    {
        Inode current = BlockManager::ReadInode(file, sb, currentInodeIndex);
        if(current.i_type != 0) return -1;

        bool found = false;
        for(int b = 0; b < 12 && !found; b++)
        {
            if(current.i_block[b] == -1) break;
            FolderBlock fb{};
            file.seekg(sb.s_block_start + (current.i_block[b] * blockSize));
            file.read(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));
            for(int e = 0; e < 4 && !found; e++)
            {
                if(fb.b_content[e].b_inodo == -1) continue;
                if(fb.b_content[e].b_name[0] == '\0') continue;
                std::string name(fb.b_content[e].b_name);
                if(name == "." || name == "..") continue;
                if(name == part) { currentInodeIndex = fb.b_content[e].b_inodo; found = true; }
            }
        }
        if(!found) return -1;
    }
    return currentInodeIndex;
}

// ======================================================
// ================= REPORTE BM_INODE ===================
// ======================================================

static void RepBmInode(const std::string& outPath, std::fstream& file,
                       SuperBlock& sb)
{
    CreateDirs(outPath);
    std::ofstream out(outPath);
    if(!out.is_open()) { std::cout << "Error: No se pudo crear el archivo\n"; return; }

    std::vector<char> bitmap(sb.s_inodes_count);
    file.seekg(sb.s_bm_inode_start);
    file.read(bitmap.data(), sb.s_inodes_count);

    for(int i = 0; i < sb.s_inodes_count; i++)
    {
        out << bitmap[i];
        if((i + 1) % 20 == 0) out << "\n";
        else if(i < sb.s_inodes_count - 1) out << " ";
    }
    out << "\n";
    out.close();
    std::cout << "Reporte bm_inode generado: " << outPath << "\n";
}

// ======================================================
// ================= REPORTE BM_BLOCK ===================
// ======================================================

static void RepBmBlock(const std::string& outPath, std::fstream& file,
                       SuperBlock& sb)
{
    CreateDirs(outPath);
    std::ofstream out(outPath);
    if(!out.is_open()) { std::cout << "Error: No se pudo crear el archivo\n"; return; }

    std::vector<char> bitmap(sb.s_blocks_count);
    file.seekg(sb.s_bm_block_start);
    file.read(bitmap.data(), sb.s_blocks_count);

    for(int i = 0; i < sb.s_blocks_count; i++)
    {
        out << bitmap[i];
        if((i + 1) % 20 == 0) out << "\n";
        else if(i < sb.s_blocks_count - 1) out << " ";
    }
    out << "\n";
    out.close();
    std::cout << "Reporte bm_block generado: " << outPath << "\n";
}

// ======================================================
// ================= REPORTE SB =========================
// ======================================================

static void RepSb(const std::string& outPath, std::fstream& file,
                  SuperBlock& sb)
{
    CreateDirs(outPath);
    std::string dotFile = outPath + ".dot";
    std::ofstream dot(dotFile);
    if(!dot.is_open()) { std::cout << "Error: No se pudo crear el archivo dot\n"; return; }

    dot << "digraph G {\n";
    dot << "  node [shape=none]\n";
    dot << "  sb [label=<\n";
    dot << "    <TABLE BORDER=\"1\" CELLBORDER=\"1\" CELLSPACING=\"0\" BGCOLOR=\"#1e2a3a\">\n";
    dot << "      <TR><TD COLSPAN=\"2\" BGCOLOR=\"#2e86de\"><FONT COLOR=\"white\"><B>SUPER BLOQUE</B></FONT></TD></TR>\n";
    dot << "      <TR><TD BGCOLOR=\"#34495e\"><FONT COLOR=\"white\">filesystem_type</FONT></TD><TD BGCOLOR=\"#2c3e50\"><FONT COLOR=\"#3498db\">" << sb.s_filesystem_type << "</FONT></TD></TR>\n";
    dot << "      <TR><TD BGCOLOR=\"#34495e\"><FONT COLOR=\"white\">inodes_count</FONT></TD><TD BGCOLOR=\"#2c3e50\"><FONT COLOR=\"#3498db\">" << sb.s_inodes_count << "</FONT></TD></TR>\n";
    dot << "      <TR><TD BGCOLOR=\"#34495e\"><FONT COLOR=\"white\">blocks_count</FONT></TD><TD BGCOLOR=\"#2c3e50\"><FONT COLOR=\"#3498db\">" << sb.s_blocks_count << "</FONT></TD></TR>\n";
    dot << "      <TR><TD BGCOLOR=\"#34495e\"><FONT COLOR=\"white\">free_blocks_count</FONT></TD><TD BGCOLOR=\"#2c3e50\"><FONT COLOR=\"#2ecc71\">" << sb.s_free_blocks_count << "</FONT></TD></TR>\n";
    dot << "      <TR><TD BGCOLOR=\"#34495e\"><FONT COLOR=\"white\">free_inodes_count</FONT></TD><TD BGCOLOR=\"#2c3e50\"><FONT COLOR=\"#2ecc71\">" << sb.s_free_inodes_count << "</FONT></TD></TR>\n";
    dot << "      <TR><TD BGCOLOR=\"#34495e\"><FONT COLOR=\"white\">mtime</FONT></TD><TD BGCOLOR=\"#2c3e50\"><FONT COLOR=\"#f39c12\">" << TimeToStr(sb.s_mtime) << "</FONT></TD></TR>\n";
    dot << "      <TR><TD BGCOLOR=\"#34495e\"><FONT COLOR=\"white\">umtime</FONT></TD><TD BGCOLOR=\"#2c3e50\"><FONT COLOR=\"#f39c12\">" << TimeToStr(sb.s_umtime) << "</FONT></TD></TR>\n";
    dot << "      <TR><TD BGCOLOR=\"#34495e\"><FONT COLOR=\"white\">mnt_count</FONT></TD><TD BGCOLOR=\"#2c3e50\"><FONT COLOR=\"#3498db\">" << sb.s_mnt_count << "</FONT></TD></TR>\n";
    dot << "      <TR><TD BGCOLOR=\"#34495e\"><FONT COLOR=\"white\">magic</FONT></TD><TD BGCOLOR=\"#2c3e50\"><FONT COLOR=\"#9b59b6\">0x" << std::hex << sb.s_magic << std::dec << "</FONT></TD></TR>\n";
    dot << "      <TR><TD BGCOLOR=\"#34495e\"><FONT COLOR=\"white\">inode_size</FONT></TD><TD BGCOLOR=\"#2c3e50\"><FONT COLOR=\"#3498db\">" << sb.s_inode_size << "</FONT></TD></TR>\n";
    dot << "      <TR><TD BGCOLOR=\"#34495e\"><FONT COLOR=\"white\">block_size</FONT></TD><TD BGCOLOR=\"#2c3e50\"><FONT COLOR=\"#3498db\">" << sb.s_block_size << "</FONT></TD></TR>\n";
    dot << "      <TR><TD BGCOLOR=\"#34495e\"><FONT COLOR=\"white\">first_ino</FONT></TD><TD BGCOLOR=\"#2c3e50\"><FONT COLOR=\"#3498db\">" << sb.s_first_ino << "</FONT></TD></TR>\n";
    dot << "      <TR><TD BGCOLOR=\"#34495e\"><FONT COLOR=\"white\">first_blo</FONT></TD><TD BGCOLOR=\"#2c3e50\"><FONT COLOR=\"#3498db\">" << sb.s_first_blo << "</FONT></TD></TR>\n";
    dot << "      <TR><TD BGCOLOR=\"#34495e\"><FONT COLOR=\"white\">bm_inode_start</FONT></TD><TD BGCOLOR=\"#2c3e50\"><FONT COLOR=\"#e74c3c\">" << sb.s_bm_inode_start << "</FONT></TD></TR>\n";
    dot << "      <TR><TD BGCOLOR=\"#34495e\"><FONT COLOR=\"white\">bm_block_start</FONT></TD><TD BGCOLOR=\"#2c3e50\"><FONT COLOR=\"#e74c3c\">" << sb.s_bm_block_start << "</FONT></TD></TR>\n";
    dot << "      <TR><TD BGCOLOR=\"#34495e\"><FONT COLOR=\"white\">inode_start</FONT></TD><TD BGCOLOR=\"#2c3e50\"><FONT COLOR=\"#e74c3c\">" << sb.s_inode_start << "</FONT></TD></TR>\n";
    dot << "      <TR><TD BGCOLOR=\"#34495e\"><FONT COLOR=\"white\">block_start</FONT></TD><TD BGCOLOR=\"#2c3e50\"><FONT COLOR=\"#e74c3c\">" << sb.s_block_start << "</FONT></TD></TR>\n";
    dot << "    </TABLE>>]\n";
    dot << "}\n";
    dot.close();

    RunDot(dotFile, outPath);
    std::cout << "Reporte sb generado: " << outPath << "\n";
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
    if(!dot.is_open()) { std::cout << "Error: No se pudo crear el archivo dot\n"; return; }

    // Colores
    const std::string colorHeaderMbr    = "#4a235a";
    const std::string colorHeaderPrim   = "#4a235a";
    const std::string colorHeaderLogica = "#c0392b";
    const std::string colorRow1         = "#ffffff";
    const std::string colorRow2         = "#e8d5f0";
    const std::string colorRowLogica1   = "#ffffff";
    const std::string colorRowLogica2   = "#f5b7b1";

    dot << "digraph G {\n";
    dot << "  graph [bgcolor=white]\n";
    dot << "  node [shape=none margin=0]\n\n";
    dot << "  mbr [label=<\n";
    dot << "  <TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"6\" "
        << "BGCOLOR=\"white\" COLOR=\"#cccccc\" WIDTH=\"300\">\n";

    // ── Encabezado MBR ──
    dot << "    <TR><TD COLSPAN=\"2\" BGCOLOR=\"" << colorHeaderMbr << "\" ALIGN=\"LEFT\">"
        << "<FONT COLOR=\"white\"><B>REPORTE DE MBR</B></FONT></TD></TR>\n";

    // ── Filas MBR ──
    auto rowMbr = [&](const std::string& key, const std::string& val, bool even) {
        std::string bg = even ? colorRow2 : colorRow1;
        dot << "    <TR>"
            << "<TD BGCOLOR=\"" << bg << "\" ALIGN=\"RIGHT\"><FONT COLOR=\"#333333\">" << key << "</FONT></TD>"
            << "<TD BGCOLOR=\"" << bg << "\" ALIGN=\"LEFT\"><FONT COLOR=\"#333333\">" << val << "</FONT></TD>"
            << "</TR>\n";
    };

    rowMbr("mbr_tamano",         std::to_string(mbr.mbr_tamano),        false);
    rowMbr("mbr_fecha_creacion", TimeToStr(mbr.mbr_fecha_creacion),     true);
    rowMbr("mbr_disk_signature", std::to_string(mbr.mbr_dsk_signature), false);

    // ── Particiones ──
    for(int i = 0; i < 4; i++)
    {
        Partition& p = mbr.mbr_partitions[i];
        if(p.part_s <= 0) continue;

        bool isLogica = (p.part_type == 'l' || p.part_type == 'L');

        std::string headerColor = isLogica ? colorHeaderLogica : colorHeaderPrim;
        std::string headerLabel = isLogica ? "Particion Logica" : "Particion";
        std::string rowA        = isLogica ? colorRowLogica1    : colorRow1;
        std::string rowB        = isLogica ? colorRowLogica2    : colorRow2;

        // Encabezado
        dot << "    <TR><TD COLSPAN=\"2\" BGCOLOR=\"" << headerColor << "\" ALIGN=\"LEFT\">"
            << "<FONT COLOR=\"white\"><B>" << headerLabel << "</B></FONT></TD></TR>\n";

        auto rowP = [&](const std::string& key, const std::string& val, bool even) {
            std::string bg = even ? rowB : rowA;
            dot << "    <TR>"
                << "<TD BGCOLOR=\"" << bg << "\" ALIGN=\"RIGHT\"><FONT COLOR=\"#333333\">" << key << "</FONT></TD>"
                << "<TD BGCOLOR=\"" << bg << "\" ALIGN=\"LEFT\"><FONT COLOR=\"#333333\">" << val << "</FONT></TD>"
                << "</TR>\n";
        };

        rowP("part_status", std::string(1, p.part_status), false);

        if(isLogica)
        {
            // Para lógicas mostrar part_next como -1 (simulado, no hay EBR real)
            rowP("part_next", "-1", true);
        }
        else
        {
            rowP("part_type", std::string(1, p.part_type), true);
        }

        rowP("part_fit",   std::string(1, p.part_fit),  isLogica ? false : false);
        rowP("part_start", std::to_string(p.part_start), true);
        rowP("part_size",  std::to_string(p.part_s),     false);
        rowP("part_name",  std::string(p.part_name),     true);
    }

    dot << "  </TABLE>>]\n";
    dot << "}\n";
    dot.close();

    RunDot(dotFile, outPath);
    std::cout << "Reporte mbr generado: " << outPath << "\n";

// ── Sección EBR ──
bool hayLogicas = false;
for(int i = 0; i < 4; i++)
    if(mbr.mbr_partitions[i].part_s > 0 &&
       (mbr.mbr_partitions[i].part_type == 'l' ||
        mbr.mbr_partitions[i].part_type == 'L'))
    { hayLogicas = true; break; }

if(hayLogicas)
{
    dot << "  ebr [label=<\n";
    dot << "  <TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"6\" "
        << "BGCOLOR=\"white\" COLOR=\"#cccccc\" WIDTH=\"300\">\n";
    dot << "    <TR><TD COLSPAN=\"2\" BGCOLOR=\"white\" ALIGN=\"LEFT\">"
        << "<FONT COLOR=\"black\"><B>EBR</B></FONT></TD></TR>\n";

    for(int i = 0; i < 4; i++)
    {
        Partition& p = mbr.mbr_partitions[i];
        if(p.part_s <= 0) continue;
        if(p.part_type != 'l' && p.part_type != 'L') continue;

        dot << "    <TR><TD COLSPAN=\"2\" BGCOLOR=\"" << colorHeaderPrim << "\" ALIGN=\"LEFT\">"
            << "<FONT COLOR=\"white\"><B>Particion</B></FONT></TD></TR>\n";

        auto rowE = [&](const std::string& key, const std::string& val, bool even) {
            std::string bg = even ? colorRow2 : colorRow1;
            dot << "    <TR>"
                << "<TD BGCOLOR=\"" << bg << "\" ALIGN=\"RIGHT\"><FONT COLOR=\"#333333\">" << key << "</FONT></TD>"
                << "<TD BGCOLOR=\"" << bg << "\" ALIGN=\"LEFT\"><FONT COLOR=\"#333333\">" << val << "</FONT></TD>"
                << "</TR>\n";
        };

        rowE("part_status", std::string(1, p.part_status), false);
        rowE("part_type",   std::string(1, p.part_type),   true);
        rowE("part_fit",    std::string(1, p.part_fit),    false);
        rowE("part_start",  std::to_string(p.part_start),  true);
        rowE("part_size",   std::to_string(p.part_s),      false);
        rowE("part_name",   std::string(p.part_name),      true);
    }

    dot << "  </TABLE>>]\n";
    dot << "  mbr -> ebr [style=invis]\n"; // alinear verticalmente
}

}

// ======================================================
// ================= REPORTE DISK =======================
// ======================================================

static void RepDisk(const std::string& outPath, std::fstream& file)
{
    CreateDirs(outPath);

    MBR mbr{};
    file.seekg(0);
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

    std::string dotFile = outPath + ".dot";
    std::ofstream dot(dotFile);
    if(!dot.is_open()) { std::cout << "Error: No se pudo crear dot\n"; return; }

    int total = mbr.mbr_tamano;

    dot << "digraph G {\n";
    dot << "  rankdir=LR\n";
    dot << "  node [shape=none]\n";
    dot << "  disk [label=<\n";
    dot << "    <TABLE BORDER=\"1\" CELLBORDER=\"1\" CELLSPACING=\"0\" BGCOLOR=\"#1a1a2e\">\n";
    dot << "      <TR><TD BGCOLOR=\"#e94560\"><FONT COLOR=\"white\"><B>MBR</B><BR/>" << sizeof(MBR) << " bytes<BR/>";
    dot << std::fixed << std::setprecision(2) << (sizeof(MBR) * 100.0 / total) << "%</FONT></TD>\n";

    int usedEnd = sizeof(MBR);
    for(int i = 0; i < 4; i++)
    {
        Partition& p = mbr.mbr_partitions[i];
        if(p.part_s <= 0) continue;

        // Espacio libre entre particiones
        if(p.part_start > usedEnd)
        {
            int free = p.part_start - usedEnd;
            double pct = free * 100.0 / total;
            dot << "      <TD BGCOLOR=\"#2c3e50\"><FONT COLOR=\"#7f8c8d\"><B>Libre</B><BR/>" << free << " bytes<BR/>";
            dot << std::fixed << std::setprecision(2) << pct << "%</FONT></TD>\n";
        }

        std::string color = (p.part_type == 'P' || p.part_type == 'p') ? "#2980b9" : "#27ae60";
        double pct = p.part_s * 100.0 / total;
        dot << "      <TD BGCOLOR=\"" << color << "\"><FONT COLOR=\"white\"><B>" << p.part_name << "</B><BR/>";
        dot << p.part_s << " bytes<BR/>";
        dot << std::fixed << std::setprecision(2) << pct << "%</FONT></TD>\n";

        usedEnd = p.part_start + p.part_s;
    }

    // Espacio libre al final
    if(usedEnd < total)
    {
        int free = total - usedEnd;
        double pct = free * 100.0 / total;
        dot << "      <TD BGCOLOR=\"#2c3e50\"><FONT COLOR=\"#7f8c8d\"><B>Libre</B><BR/>" << free << " bytes<BR/>";
        dot << std::fixed << std::setprecision(2) << pct << "%</FONT></TD>\n";
    }

    dot << "    </TR></TABLE>>]\n";
    dot << "}\n";
    dot.close();

    RunDot(dotFile, outPath);
    std::cout << "Reporte disk generado: " << outPath << "\n";
}

// ======================================================
// ================= REPORTE INODE ======================
// ======================================================

static void RepInode(const std::string& outPath, std::fstream& file,
                     SuperBlock& sb)
{
    CreateDirs(outPath);

    std::vector<char> bitmap(sb.s_inodes_count);
    file.seekg(sb.s_bm_inode_start);
    file.read(bitmap.data(), sb.s_inodes_count);

    std::string dotFile = outPath + ".dot";
    std::ofstream dot(dotFile);
    if(!dot.is_open()) { std::cout << "Error: No se pudo crear dot\n"; return; }

    dot << "digraph G {\n";
    dot << "  node [shape=none]\n";

    for(int i = 0; i < sb.s_inodes_count; i++)
    {
        if(bitmap[i] != '1') continue;

        Inode inode = BlockManager::ReadInode(file, sb, i);
        std::string tipo = (inode.i_type == 0) ? "Carpeta" : "Archivo";
        std::string color = (inode.i_type == 0) ? "#2980b9" : "#27ae60";

        dot << "  inode" << i << " [label=<\n";
        dot << "    <TABLE BORDER=\"1\" CELLBORDER=\"1\" CELLSPACING=\"0\" BGCOLOR=\"#1a1a2e\">\n";
        dot << "      <TR><TD COLSPAN=\"2\" BGCOLOR=\"" << color << "\"><FONT COLOR=\"white\"><B>INODO " << i << " - " << tipo << "</B></FONT></TD></TR>\n";
        dot << "      <TR><TD BGCOLOR=\"#16213e\"><FONT COLOR=\"white\">i_uid</FONT></TD><TD BGCOLOR=\"#0f3460\"><FONT COLOR=\"#3498db\">" << inode.i_uid << "</FONT></TD></TR>\n";
        dot << "      <TR><TD BGCOLOR=\"#16213e\"><FONT COLOR=\"white\">i_gid</FONT></TD><TD BGCOLOR=\"#0f3460\"><FONT COLOR=\"#3498db\">" << inode.i_gid << "</FONT></TD></TR>\n";
        dot << "      <TR><TD BGCOLOR=\"#16213e\"><FONT COLOR=\"white\">i_size</FONT></TD><TD BGCOLOR=\"#0f3460\"><FONT COLOR=\"#2ecc71\">" << inode.i_size << "</FONT></TD></TR>\n";
        dot << "      <TR><TD BGCOLOR=\"#16213e\"><FONT COLOR=\"white\">i_atime</FONT></TD><TD BGCOLOR=\"#0f3460\"><FONT COLOR=\"#f39c12\">" << TimeToStr(inode.i_atime) << "</FONT></TD></TR>\n";
        dot << "      <TR><TD BGCOLOR=\"#16213e\"><FONT COLOR=\"white\">i_ctime</FONT></TD><TD BGCOLOR=\"#0f3460\"><FONT COLOR=\"#f39c12\">" << TimeToStr(inode.i_ctime) << "</FONT></TD></TR>\n";
        dot << "      <TR><TD BGCOLOR=\"#16213e\"><FONT COLOR=\"white\">i_mtime</FONT></TD><TD BGCOLOR=\"#0f3460\"><FONT COLOR=\"#f39c12\">" << TimeToStr(inode.i_mtime) << "</FONT></TD></TR>\n";
        dot << "      <TR><TD BGCOLOR=\"#16213e\"><FONT COLOR=\"white\">i_type</FONT></TD><TD BGCOLOR=\"#0f3460\"><FONT COLOR=\"white\">" << tipo << "</FONT></TD></TR>\n";
        dot << "      <TR><TD BGCOLOR=\"#16213e\"><FONT COLOR=\"white\">i_perm</FONT></TD><TD BGCOLOR=\"#0f3460\"><FONT COLOR=\"#e74c3c\">" << inode.i_perm << "</FONT></TD></TR>\n";

        for(int b = 0; b < 15; b++)
        {
            if(inode.i_block[b] == -1) continue;
            std::string label = (b < 12) ? "Directo " + std::to_string(b) :
                                (b == 12) ? "Simple Ind." :
                                (b == 13) ? "Doble Ind." : "Triple Ind.";
            dot << "      <TR><TD BGCOLOR=\"#16213e\"><FONT COLOR=\"white\">i_block[" << b << "] " << label << "</FONT></TD><TD BGCOLOR=\"#0f3460\"><FONT COLOR=\"#9b59b6\">" << inode.i_block[b] << "</FONT></TD></TR>\n";
        }

        dot << "    </TABLE>>]\n\n";
    }

    // Conectar inodos usados en orden
    bool first = true;
    int prev = -1;
    for(int i = 0; i < sb.s_inodes_count; i++)
    {
        if(bitmap[i] != '1') continue;
        if(prev != -1) dot << "  inode" << prev << " -> inode" << i << "\n";
        prev = i;
    }

    dot << "}\n";
    dot.close();

    RunDot(dotFile, outPath);
    std::cout << "Reporte inode generado: " << outPath << "\n";
}

// ======================================================
// ================= REPORTE BLOCK ======================
// ======================================================

static void RepBlock(const std::string& outPath, std::fstream& file,
                     SuperBlock& sb)
{
    CreateDirs(outPath);

    std::vector<char> bitmap(sb.s_blocks_count);
    file.seekg(sb.s_bm_block_start);
    file.read(bitmap.data(), sb.s_blocks_count);

    std::vector<char> inodeBitmap(sb.s_inodes_count);
    file.seekg(sb.s_bm_inode_start);
    file.read(inodeBitmap.data(), sb.s_inodes_count);

    std::string dotFile = outPath + ".dot";
    std::ofstream dot(dotFile);
    if(!dot.is_open()) { std::cout << "Error: No se pudo crear dot\n"; return; }

    dot << "digraph G {\n";
    dot << "  node [shape=none]\n";

    for(int i = 0; i < sb.s_blocks_count; i++)
    {
        if(bitmap[i] != '1') continue;

        // Determinar tipo de bloque leyendo inodos
        bool isFolderBlock = false;
        bool isFileBlock   = false;
        bool isPointerBlock = false;

        for(int j = 0; j < sb.s_inodes_count && !isFolderBlock && !isFileBlock; j++)
        {
            if(inodeBitmap[j] != '1') continue;
            Inode inode = BlockManager::ReadInode(file, sb, j);
            for(int b = 0; b < 12; b++)
            {
                if(inode.i_block[b] == i)
                {
                    if(inode.i_type == 0) isFolderBlock = true;
                    else isFileBlock = true;
                    break;
                }
            }
            if(inode.i_block[12] == i || inode.i_block[13] == i || inode.i_block[14] == i)
                isPointerBlock = true;
        }

        if(isFolderBlock)
        {
            FolderBlock fb{};
            file.seekg(sb.s_block_start + (i * sizeof(FileBlock)));
            file.read(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));

            dot << "  block" << i << " [label=<\n";
            dot << "    <TABLE BORDER=\"1\" CELLBORDER=\"1\" CELLSPACING=\"0\" BGCOLOR=\"#1a1a2e\">\n";
            dot << "      <TR><TD COLSPAN=\"2\" BGCOLOR=\"#2980b9\"><FONT COLOR=\"white\"><B>BLOQUE CARPETA " << i << "</B></FONT></TD></TR>\n";
            for(int e = 0; e < 4; e++)
            {
                if(fb.b_content[e].b_inodo == -1) continue;
                dot << "      <TR><TD BGCOLOR=\"#16213e\"><FONT COLOR=\"white\">" << fb.b_content[e].b_name << "</FONT></TD>";
                dot << "<TD BGCOLOR=\"#0f3460\"><FONT COLOR=\"#3498db\">inodo " << fb.b_content[e].b_inodo << "</FONT></TD></TR>\n";
            }
            dot << "    </TABLE>>]\n\n";
        }
        else if(isFileBlock)
        {
            FileBlock fileBlock{};
            file.seekg(sb.s_block_start + (i * sizeof(FileBlock)));
            file.read(reinterpret_cast<char*>(&fileBlock), sizeof(FileBlock));

            std::string content(fileBlock.b_content, 64);
            // Escapar caracteres especiales para graphviz
            std::string escaped = "";
            for(char c : content)
            {
                if(c == '<') escaped += "&lt;";
                else if(c == '>') escaped += "&gt;";
                else if(c == '&') escaped += "&amp;";
                else if(c == '"') escaped += "&quot;";
                else if(c == '\0') break;
                else escaped += c;
            }

            dot << "  block" << i << " [label=<\n";
            dot << "    <TABLE BORDER=\"1\" CELLBORDER=\"1\" CELLSPACING=\"0\" BGCOLOR=\"#1a1a2e\">\n";
            dot << "      <TR><TD BGCOLOR=\"#27ae60\"><FONT COLOR=\"white\"><B>BLOQUE ARCHIVO " << i << "</B></FONT></TD></TR>\n";
            dot << "      <TR><TD BGCOLOR=\"#0f3460\"><FONT COLOR=\"#2ecc71\">" << escaped << "</FONT></TD></TR>\n";
            dot << "    </TABLE>>]\n\n";
        }
        else
        {
            dot << "  block" << i << " [label=<\n";
            dot << "    <TABLE BORDER=\"1\" CELLBORDER=\"1\" CELLSPACING=\"0\" BGCOLOR=\"#1a1a2e\">\n";
            dot << "      <TR><TD BGCOLOR=\"#8e44ad\"><FONT COLOR=\"white\"><B>BLOQUE APUNTADOR " << i << "</B></FONT></TD></TR>\n";
            dot << "    </TABLE>>]\n\n";
        }
    }

    // Conectar bloques en orden
    int prev = -1;
    for(int i = 0; i < sb.s_blocks_count; i++)
    {
        if(bitmap[i] != '1') continue;
        if(prev != -1) dot << "  block" << prev << " -> block" << i << "\n";
        prev = i;
    }

    dot << "}\n";
    dot.close();

    RunDot(dotFile, outPath);
    std::cout << "Reporte block generado: " << outPath << "\n";
}

// ======================================================
// ================= REPORTE FILE =======================
// ======================================================

static void RepFile(const std::string& outPath, std::fstream& file,
                    SuperBlock& sb, const std::string& pathFileLs)
{
    CreateDirs(outPath);

    int inodeIndex = FindInodeByPath(file, sb, pathFileLs);
    if(inodeIndex == -1)
    {
        std::cout << "Error: Archivo no encontrado: " << pathFileLs << "\n";
        return;
    }

    Inode inode = BlockManager::ReadInode(file, sb, inodeIndex);
    if(inode.i_type != 1)
    {
        std::cout << "Error: La ruta no es un archivo\n";
        return;
    }

    std::string content = BlockManager::ReadFileContent(file, sb, inode);
    std::string fileName = pathFileLs.substr(pathFileLs.find_last_of('/') + 1);

    // Escapar para graphviz
    std::string escaped = "";
    for(char c : content)
    {
        if(c == '<') escaped += "&lt;";
        else if(c == '>') escaped += "&gt;";
        else if(c == '&') escaped += "&amp;";
        else if(c == '"') escaped += "&quot;";
        else if(c == '\n') escaped += "<BR/>";
        else escaped += c;
    }

    std::string dotFile = outPath + ".dot";
    std::ofstream dot(dotFile);
    dot << "digraph G {\n";
    dot << "  node [shape=none]\n";
    dot << "  file [label=<\n";
    dot << "    <TABLE BORDER=\"1\" CELLBORDER=\"1\" CELLSPACING=\"0\" BGCOLOR=\"#1a1a2e\">\n";
    dot << "      <TR><TD BGCOLOR=\"#27ae60\"><FONT COLOR=\"white\"><B>" << fileName << "</B></FONT></TD></TR>\n";
    dot << "      <TR><TD BGCOLOR=\"#0f3460\"><FONT COLOR=\"#2ecc71\">" << escaped << "</FONT></TD></TR>\n";
    dot << "    </TABLE>>]\n";
    dot << "}\n";
    dot.close();

    RunDot(dotFile, outPath);
    std::cout << "Reporte file generado: " << outPath << "\n";
}

// ======================================================
// ================= REPORTE LS =========================
// ======================================================

static void RepLs(const std::string& outPath, std::fstream& file,
                  SuperBlock& sb, const std::string& pathFileLs)
{
    CreateDirs(outPath);

    int inodeIndex = pathFileLs == "/" ? 0 : FindInodeByPath(file, sb, pathFileLs);
    if(inodeIndex == -1)
    {
        std::cout << "Error: Ruta no encontrada: " << pathFileLs << "\n";
        return;
    }

    Inode inode = BlockManager::ReadInode(file, sb, inodeIndex);
    if(inode.i_type != 0)
    {
        std::cout << "Error: La ruta no es una carpeta\n";
        return;
    }

    std::string dotFile = outPath + ".dot";
    std::ofstream dot(dotFile);
    dot << "digraph G {\n";
    dot << "  node [shape=none]\n";
    dot << "  ls [label=<\n";
    dot << "    <TABLE BORDER=\"1\" CELLBORDER=\"1\" CELLSPACING=\"0\" BGCOLOR=\"#1a1a2e\">\n";
    dot << "      <TR>\n";
    dot << "        <TD BGCOLOR=\"#e94560\"><FONT COLOR=\"white\"><B>Permisos</B></FONT></TD>\n";
    dot << "        <TD BGCOLOR=\"#e94560\"><FONT COLOR=\"white\"><B>Propietario</B></FONT></TD>\n";
    dot << "        <TD BGCOLOR=\"#e94560\"><FONT COLOR=\"white\"><B>Grupo</B></FONT></TD>\n";
    dot << "        <TD BGCOLOR=\"#e94560\"><FONT COLOR=\"white\"><B>Tipo</B></FONT></TD>\n";
    dot << "        <TD BGCOLOR=\"#e94560\"><FONT COLOR=\"white\"><B>Fecha Mod.</B></FONT></TD>\n";
    dot << "        <TD BGCOLOR=\"#e94560\"><FONT COLOR=\"white\"><B>Fecha Crea.</B></FONT></TD>\n";
    dot << "        <TD BGCOLOR=\"#e94560\"><FONT COLOR=\"white\"><B>Nombre</B></FONT></TD>\n";
    dot << "      </TR>\n";

    int blockSize = sizeof(FileBlock);
    for(int b = 0; b < 12; b++)
    {
        if(inode.i_block[b] == -1) break;
        FolderBlock fb{};
        file.seekg(sb.s_block_start + (inode.i_block[b] * blockSize));
        file.read(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));

        for(int e = 0; e < 4; e++)
        {
            if(fb.b_content[e].b_inodo == -1) continue;
            if(fb.b_content[e].b_name[0] == '\0') continue;
            std::string name(fb.b_content[e].b_name);
            if(name == "." || name == "..") continue;

            Inode entryInode = BlockManager::ReadInode(file, sb, fb.b_content[e].b_inodo);
            std::string tipo = (entryInode.i_type == 0) ? "Carpeta" : "Archivo";
            std::string bgColor = (entryInode.i_type == 0) ? "#16213e" : "#0d1b2a";
            std::string typeColor = (entryInode.i_type == 0) ? "#2980b9" : "#27ae60";

            int perm = entryInode.i_perm;
            int permU = (perm / 100) % 10;
            int permG = (perm / 10)  % 10;
            int permO = perm         % 10;

            auto permStr = [](int p) -> std::string {
                std::string s = "";
                s += (p >= 4) ? "r" : "-";
                s += (p == 2 || p == 3 || p == 6 || p == 7) ? "w" : "-";
                s += (p == 1 || p == 3 || p == 5 || p == 7) ? "x" : "-";
                return s;
            };

            std::string perms = permStr(permU) + permStr(permG) + permStr(permO);

            dot << "      <TR>\n";
            dot << "        <TD BGCOLOR=\"" << bgColor << "\"><FONT COLOR=\"#e74c3c\">" << perms << "</FONT></TD>\n";
            dot << "        <TD BGCOLOR=\"" << bgColor << "\"><FONT COLOR=\"#3498db\">" << entryInode.i_uid << "</FONT></TD>\n";
            dot << "        <TD BGCOLOR=\"" << bgColor << "\"><FONT COLOR=\"#3498db\">" << entryInode.i_gid << "</FONT></TD>\n";
            dot << "        <TD BGCOLOR=\"" << bgColor << "\"><FONT COLOR=\"" << typeColor << "\">" << tipo << "</FONT></TD>\n";
            dot << "        <TD BGCOLOR=\"" << bgColor << "\"><FONT COLOR=\"#f39c12\">" << TimeToStr(entryInode.i_mtime) << "</FONT></TD>\n";
            dot << "        <TD BGCOLOR=\"" << bgColor << "\"><FONT COLOR=\"#f39c12\">" << TimeToStr(entryInode.i_ctime) << "</FONT></TD>\n";
            dot << "        <TD BGCOLOR=\"" << bgColor << "\"><FONT COLOR=\"white\">" << name << "</FONT></TD>\n";
            dot << "      </TR>\n";
        }
    }

    dot << "    </TABLE>>]\n";
    dot << "}\n";
    dot.close();

    RunDot(dotFile, outPath);
    std::cout << "Reporte ls generado: " << outPath << "\n";
}

// ======================================================
// ================= REPORTE TREE =======================
// ======================================================

static int treeNodeCount = 0;

static void TreeTraverse(std::ofstream& dot, std::fstream& file,
                         SuperBlock& sb, int inodeIndex,
                         const std::string& name, int parentNodeId)
{
    int myNodeId = treeNodeCount++;
    Inode inode = BlockManager::ReadInode(file, sb, inodeIndex);

    std::string tipo = (inode.i_type == 0) ? "Carpeta" : "Archivo";
    std::string color = (inode.i_type == 0) ? "#2980b9" : "#27ae60";

    dot << "  node" << myNodeId << " [label=<\n";
    dot << "    <TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"2\" BGCOLOR=\"#1a1a2e\">\n";
    dot << "      <TR><TD BGCOLOR=\"" << color << "\"><FONT COLOR=\"white\"><B>" << name << "</B></FONT></TD></TR>\n";
    dot << "      <TR><TD BGCOLOR=\"#0f3460\"><FONT COLOR=\"#95a5a6\">inodo: " << inodeIndex << " | tipo: " << tipo << " | perm: " << inode.i_perm << "</FONT></TD></TR>\n";
    dot << "    </TABLE>>]\n";

    if(parentNodeId >= 0)
        dot << "  node" << parentNodeId << " -> node" << myNodeId << "\n";

    if(inode.i_type == 0)
    {
        int blockSize = sizeof(FileBlock);
        for(int b = 0; b < 12; b++)
        {
            if(inode.i_block[b] == -1) break;
            FolderBlock fb{};
            file.seekg(sb.s_block_start + (inode.i_block[b] * blockSize));
            file.read(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));

            for(int e = 0; e < 4; e++)
            {
                if(fb.b_content[e].b_inodo == -1) continue;
                if(fb.b_content[e].b_name[0] == '\0') continue;
                std::string entryName(fb.b_content[e].b_name);
                if(entryName == "." || entryName == "..") continue;

                TreeTraverse(dot, file, sb, fb.b_content[e].b_inodo,
                             entryName, myNodeId);
            }
        }
    }
}

static void RepTree(const std::string& outPath, std::fstream& file,
                    SuperBlock& sb)
{
    CreateDirs(outPath);
    treeNodeCount = 0;

    std::string dotFile = outPath + ".dot";
    std::ofstream dot(dotFile);
    dot << "digraph G {\n";
    dot << "  rankdir=TB\n";
    dot << "  node [shape=none]\n";

    TreeTraverse(dot, file, sb, 0, "/", -1);

    dot << "}\n";
    dot.close();

    RunDot(dotFile, outPath);
    std::cout << "Reporte tree generado: " << outPath << "\n";
}

// ======================================================
// ================= REP (DISPATCHER) ===================
// ======================================================

void Rep(const std::string& name,
         const std::string& path,
         const std::string& id,
         const std::string& pathFileLs)
{
    // Validar nombre
    std::vector<std::string> validNames = {
        "mbr","disk","inode","block","bm_inode","bm_block","tree","sb","file","ls"
    };
    std::string nameLower = name;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

    bool valid = false;
    for(auto& n : validNames) if(n == nameLower) { valid = true; break; }
    if(!valid)
    {
        std::cout << "Error: Nombre de reporte no válido: " << name << "\n";
        return;
    }

    // Buscar partición montada
    MountedPartition* mounted = nullptr;
    for(auto& m : mountedList)
        if(m.id == id) { mounted = &m; break; }

    if(!mounted)
    {
        std::cout << "Error: ID no encontrado: " << id << "\n";
        return;
    }

    auto file = FileUtils::OpenFile(mounted->path);
    if(!file.is_open())
    {
        std::cout << "Error: No se pudo abrir el disco\n";
        return;
    }

    MBR mbr{};
    file.seekg(0);
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

    Partition* partition = nullptr;
    for(int i = 0; i < 4; i++)
    {
        if(mbr.mbr_partitions[i].part_s > 0 &&
           std::string(mbr.mbr_partitions[i].part_name) == mounted->name)
        {
            partition = &mbr.mbr_partitions[i];
            break;
        }
    }

    SuperBlock sb{};
    if(partition)
    {
        file.seekg(partition->part_start);
        file.read(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));
    }

    if(nameLower == "bm_inode")  RepBmInode(path, file, sb);
    else if(nameLower == "bm_block") RepBmBlock(path, file, sb);
    else if(nameLower == "sb")   RepSb(path, file, sb);
    else if(nameLower == "mbr")  RepMbr(path, file);
    else if(nameLower == "disk") RepDisk(path, file);
    else if(nameLower == "inode") RepInode(path, file, sb);
    else if(nameLower == "block") RepBlock(path, file, sb);
    else if(nameLower == "tree") RepTree(path, file, sb);
    else if(nameLower == "file")
    {
        if(pathFileLs.empty()) { std::cout << "Error: file requiere -path_file_ls\n"; return; }
        RepFile(path, file, sb, pathFileLs);
    }
    else if(nameLower == "ls")
    {
        if(pathFileLs.empty()) { std::cout << "Error: ls requiere -path_file_ls\n"; return; }
        RepLs(path, file, sb, pathFileLs);
    }

    file.close();
}

} // namespace RepManager