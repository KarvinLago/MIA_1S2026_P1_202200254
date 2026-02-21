#pragma once
#include <cstdint>
#include <ctime>

#pragma pack(push,1)

struct Partition {
    char part_status;      // 0 libre, 1 montada
    char part_type;        // P o E
    char part_fit;         // B, F o W
    int32_t part_start;
    int32_t part_s;
    char part_name[16];
    int32_t part_correlative;
    char part_id[4];
};

struct MBR {
    int32_t mbr_tamano;
    time_t  mbr_fecha_creacion;
    int32_t mbr_dsk_signature;
    char    dsk_fit;
    Partition mbr_partitions[4];
};


// ================= EXT2 STRUCTURES =================

struct SuperBlock {
    int32_t s_filesystem_type;
    int32_t s_inodes_count;
    int32_t s_blocks_count;
    int32_t s_free_blocks_count;
    int32_t s_free_inodes_count;
    time_t  s_mtime;
    time_t  s_umtime;
    int32_t s_mnt_count;
    int32_t s_magic;
    int32_t s_inode_size;
    int32_t s_block_size;
    int32_t s_first_ino;
    int32_t s_first_blo;
    int32_t s_bm_inode_start;
    int32_t s_bm_block_start;
    int32_t s_inode_start;
    int32_t s_block_start;
};

struct Inode {
    int32_t i_uid;
    int32_t i_gid;
    int32_t i_size;
    time_t  i_atime;
    time_t  i_ctime;
    time_t  i_mtime;
    int32_t i_block[15];
    char    i_type;   // 0 carpeta, 1 archivo
    int32_t i_perm;
};

struct FolderContent {
    char b_name[12];
    int32_t b_inodo;
};

struct FolderBlock {
    FolderContent b_content[4];
};

struct FileBlock {
    char b_content[64];
};


#pragma pack(pop)