#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>



typedef struct {
    uint8_t version;
    uint8_t flags[3];
    uint64_t creation_time;
    uint64_t modification_time;
    uint32_t timescale;
    uint64_t duration;

} MVHDBox;

bool is_container_atom(const char *type) {
    const char *containers[] = {"moov", "trak", "mdia", "minf", "stbl", NULL};
    for (int i = 0; containers[i]; i++) {
        if (strcmp(type, containers[i]) == 0) return true;
    }
    return false;
}

// Read 4 bytes big-endian to uint32
uint32_t read_u32(FILE *fp) {
    uint8_t bytes[4];
    if (fread(bytes, 1, 4, fp) != 4) return 0;
    return (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
}

// Read 8 bytes big-endian to uint64
uint64_t read_u64(FILE *fp) {
    uint8_t bytes[8];
    if (fread(bytes, 1, 8, fp) != 8) return 0;
    return ((uint64_t)bytes[0] << 56) | ((uint64_t)bytes[1] << 48) |
           ((uint64_t)bytes[2] << 40) | ((uint64_t)bytes[3] << 32) |
           ((uint64_t)bytes[4] << 24) | ((uint64_t)bytes[5] << 16) |
           ((uint64_t)bytes[6] << 8) | (uint64_t)bytes[7];
}

// Read 4-character type
void read_type(FILE *fp, char *type) {
    if (fread(type, 1, 4, fp) != 4) {
        strcpy(type, "????");
    }
    type[4] = '\0'; // Null terminate
}

uint64_t read_atom_size(FILE *fp) {
    uint32_t size = read_u32(fp);
    if (size == 1) {
        // 64-bit extended size
        return read_u64(fp);
    } else if (size < 8) {
        return 0;
    }
    return size;
}

void mvhd_handler(FILE *fp, uint64_t size) {
    MVHDBox mvhd;
    long start = ftell(fp); 
    fread(&mvhd.version, sizeof(uint8_t), 1, fp);
    fread(mvhd.flags, sizeof(uint8_t), 3, fp);
    if (mvhd.version == 1) {
        mvhd.creation_time = read_u64(fp);
        mvhd.modification_time = read_u64(fp);
        mvhd.timescale = read_u32(fp);
        mvhd.duration = read_u64(fp);
    } else {
        mvhd.creation_time = (uint64_t) read_u32(fp);
        mvhd.modification_time = (uint64_t) read_u32(fp);
        mvhd.timescale = (uint32_t) read_u32(fp);
        mvhd.duration = (uint32_t) read_u32(fp);
    }
    printf("Version: %u\n", mvhd.version);
    printf("Time Scale: %u\n", mvhd.timescale);
    printf("Duration (units): %llu\n", mvhd.duration);
    printf("Duration (seconds): %.2f\n", (double)mvhd.duration / mvhd.timescale);
    fseek(fp, start + size, SEEK_SET);
}

void parse_atoms(FILE *fp , uint64_t parent_size , int depth) {
    uint64_t parsed_bytes = 0;
    while (parsed_bytes < parent_size) {
        long atom_start = ftell(fp);
        if (atom_start == -1) break;
        uint64_t size = read_atom_size(fp);
        if (size  < 8) break;
        char type[5];
        read_type(fp, type);
        if(parsed_bytes + size > parent_size) {
            printf("Warning: Parsed bytes exceed parent size at offset %ld\n", atom_start);
            break;
        }
        
        printf("%*sAtom: %s (size: %llu, offset: %ld)\n", depth * 2, "", type, size, atom_start);
        if (strcmp(type, "mvhd") == 0) {
            mvhd_handler(fp, size-8);
        }
        else if(is_container_atom(type)) {
            // If it's a container atom, recursively parse its contents
            parse_atoms(fp, size - 8, depth + 1);
        } else {
            // Skip the rest of the atom
            fseek(fp, size - 8, SEEK_CUR);
        }
        parsed_bytes += size;
    }    
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <input.mp4>\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        perror("Failed to open file");
        return 1;
    }
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);//fix:didnt rewind to beginning
    fseek(fp, 0, SEEK_SET); // Rewind to beginning

    parse_atoms(fp, file_size, 0);
    fclose(fp);
    return 0;
}
