#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(dir) _mkdir(dir)
#else
#include <sys/stat.h>
#define MKDIR(dir) mkdir(dir, 0777)
#endif

uint32_t read_u32be(const uint8_t* data) {
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) | (uint32_t)data[3];
}

void write_u32be(uint8_t* data, uint32_t val) {
    data[0] = (val >> 24) & 0xFF;
    data[1] = (val >> 16) & 0xFF;
    data[2] = (val >> 8) & 0xFF;
    data[3] = val & 0xFF;
}

uint16_t read_u16be(const uint8_t* data) {
    return ((uint16_t)data[0] << 8) | (uint16_t)data[1];
}

// Helper to get base name without extension
void get_basename(const char* path, char* basename, size_t max_len) {
    const char* last_slash = strrchr(path, '/');
    const char* last_bslash = strrchr(path, '\\');
    const char* start = path;
    if (last_slash && last_slash > start) start = last_slash + 1;
    if (last_bslash && last_bslash > start) start = last_bslash + 1;
    
    strncpy(basename, start, max_len);
    basename[max_len - 1] = '\0';
    
    char* dot = strrchr(basename, '.');
    if (dot) {
        *dot = '\0';
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <input.ims> [output_dir]\n", argv[0]);
        return 1;
    }
    
    const char* input_file = argv[1];
    char default_out_dir[512];
    const char* output_dir = NULL;
    
    if (argc >= 3) {
        output_dir = argv[2];
    } else {
        strncpy(default_out_dir, input_file, sizeof(default_out_dir));
        default_out_dir[sizeof(default_out_dir) - 1] = '\0';
        char* last_slash = strrchr(default_out_dir, '/');
        char* last_bslash = strrchr(default_out_dir, '\\');
        char* slash = (last_slash > last_bslash) ? last_slash : last_bslash;
        if (slash) {
            *slash = '\0';
        } else {
            strcpy(default_out_dir, ".");
        }
        output_dir = default_out_dir;
    }
    
    FILE* f = fopen(input_file, "rb");
    if (!f) {
        printf("Error: Could not open %s\n", input_file);
        return 1;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    uint8_t* data = (uint8_t*)malloc(size);
    if (!data) {
        fclose(f);
        return 1;
    }
    
    if (fread(data, 1, size, f) != size) {
        printf("Error: Could not read file\n");
        free(data);
        fclose(f);
        return 1;
    }
    fclose(f);
    
    if (size < 8 || memcmp(data, "IMSB", 4) != 0) {
        printf("Error: Not a valid IMS file\n");
        free(data);
        return 1;
    }
    
    uint32_t imsb_size = read_u32be(data + 4);
    if (imsb_size > size) {
        imsb_size = size;
    }
    
    char ims_basename[256];
    get_basename(input_file, ims_basename, sizeof(ims_basename));
    
    char dir_path[512];
    const char* sep_dir = "/";
    if (output_dir[strlen(output_dir) - 1] == '/' || output_dir[strlen(output_dir) - 1] == '\\') {
        sep_dir = "";
    }
    sprintf(dir_path, "%s%s%s.soundir", output_dir, sep_dir, ims_basename);
    MKDIR(dir_path);
    
    uint32_t offset = 8;
    while (offset + 8 <= imsb_size) {
        const uint8_t* chunk_id = data + offset;
        uint32_t chunk_size = read_u32be(data + offset + 4);
        
        if (chunk_size < 8) break;
        if (offset + chunk_size > imsb_size) break;
        
        if (memcmp(chunk_id, "SOUN", 4) == 0) {
            uint32_t sub_offset = offset + 8;
            uint32_t end_offset = offset + chunk_size;
            
            int sound_id = -1;
            char sound_name[256] = {0};
            
            // First pass: find SIDN and NAME
            uint32_t temp_offset = sub_offset;
            while (temp_offset + 8 <= end_offset) {
                const uint8_t* sub_id = data + temp_offset;
                uint32_t sub_size = read_u32be(data + temp_offset + 4);
                if (sub_size < 8 || temp_offset + sub_size > end_offset) break;
                
                if (memcmp(sub_id, "SIDN", 4) == 0 && sub_size >= 10) {
                    sound_id = read_u16be(data + temp_offset + 8);
                } else if (memcmp(sub_id, "NAME", 4) == 0) {
                    uint32_t name_len = sub_size - 8;
                    if (name_len > sizeof(sound_name) - 1) name_len = sizeof(sound_name) - 1;
                    memcpy(sound_name, data + temp_offset + 8, name_len);
                    sound_name[name_len] = '\0';
                }
                
                temp_offset += sub_size;
            }
            
            char out_filename[512];
            if (strlen(sound_name) > 0) {
                sprintf(out_filename, "%s/%d_%s.soun", dir_path, sound_id, sound_name);
            } else {
                sprintf(out_filename, "%s/%s_sound%d.soun", dir_path, ims_basename, sound_id);
            }
            
            // Second pass: count kept chunks and calculate their total size
            int kept_count = 0;
            uint32_t kept_size = 0;
            
            temp_offset = sub_offset;
            while (temp_offset + 8 <= end_offset) {
                const uint8_t* sub_id = data + temp_offset;
                uint32_t sub_size = read_u32be(data + temp_offset + 4);
                if (sub_size < 8 || temp_offset + sub_size > end_offset) break;
                
                if (memcmp(sub_id, "GMD ", 4) == 0 || 
                    memcmp(sub_id, "ROL ", 4) == 0 || 
                    memcmp(sub_id, "ADL ", 4) == 0) {
                    
                    kept_count++;
                    kept_size += sub_size;
                }
                
                temp_offset += sub_size;
            }
            
            if (kept_count == 0) {
                offset += chunk_size;
                continue;
            }
            
            // Create a buffer for the output file
            uint32_t out_capacity = (kept_count == 1) ? (8 + kept_size) : (16 + kept_size);
            uint8_t* out_data = (uint8_t*)malloc(out_capacity);
            uint32_t out_offset = 0;
            
            // Write SOUN wrapper (size INCLUDES 8-byte header)
            memcpy(out_data + out_offset, "SOUN", 4);
            write_u32be(out_data + out_offset + 4, out_capacity);
            out_offset += 8;
            
            if (kept_count == 1) {
                // If only 1 chunk, write it as a MIDI block
                temp_offset = sub_offset;
                while (temp_offset + 8 <= end_offset) {
                    const uint8_t* sub_id = data + temp_offset;
                    uint32_t sub_size = read_u32be(data + temp_offset + 4);
                    if (sub_size < 8 || temp_offset + sub_size > end_offset) break;
                    
                    if (memcmp(sub_id, "GMD ", 4) == 0 || 
                        memcmp(sub_id, "ROL ", 4) == 0 || 
                        memcmp(sub_id, "ADL ", 4) == 0) {
                        
                        memcpy(out_data + out_offset, "MIDI", 4);
                        // MIDI size field EXCLUDES 8-byte header
                        write_u32be(out_data + out_offset + 4, sub_size - 8);
                        memcpy(out_data + out_offset + 8, data + temp_offset + 8, sub_size - 8);
                        out_offset += sub_size;
                        break;
                    }
                    
                    temp_offset += sub_size;
                }
            } else {
                // If multiple chunks, wrap them in a SOU block
                memcpy(out_data + out_offset, "SOU ", 4);
                // SOU size field EXCLUDES 8-byte header
                write_u32be(out_data + out_offset + 4, kept_size);
                out_offset += 8;
                
                temp_offset = sub_offset;
                while (temp_offset + 8 <= end_offset) {
                    const uint8_t* sub_id = data + temp_offset;
                    uint32_t sub_size = read_u32be(data + temp_offset + 4);
                    if (sub_size < 8 || temp_offset + sub_size > end_offset) break;
                    
                    if (memcmp(sub_id, "GMD ", 4) == 0 || 
                        memcmp(sub_id, "ROL ", 4) == 0 || 
                        memcmp(sub_id, "ADL ", 4) == 0) {
                        
                        memcpy(out_data + out_offset, data + temp_offset, 4);
                        // Inner chunks size field EXCLUDES 8-byte header
                        write_u32be(out_data + out_offset + 4, sub_size - 8);
                        memcpy(out_data + out_offset + 8, data + temp_offset + 8, sub_size - 8);
                        out_offset += sub_size;
                    }
                    
                    temp_offset += sub_size;
                }
            }
            
            FILE* out_f = fopen(out_filename, "wb");
            if (out_f) {
                fwrite(out_data, 1, out_offset, out_f);
                fclose(out_f);
                printf("Written %s\n", out_filename);
            } else {
                printf("Error: Could not open %s for writing\n", out_filename);
            }
            
            free(out_data);
        }
        
        offset += chunk_size;
    }
    
    free(data);
    
#ifdef _WIN32
    if (argc == 2) {
        printf("\nAppuyez sur Entree pour quitter...");
        getchar();
    }
#endif
    
    return 0;
}
