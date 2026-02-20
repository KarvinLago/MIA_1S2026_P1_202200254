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

#pragma pack(pop)