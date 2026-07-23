#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: kogxor <input_file> [key_0_to_255] [output_file]\n");
        printf("Example: kogxor MT32_CONTROL.ROM\n");
        printf("Example: kogxor MT32_CONTROL.ROM 39\n");
        return 1;
    }

    const char* input_path = argv[1];
    int key = 39;
    if (argc >= 3) {
        key = atoi(argv[2]);
    }

    if (key < 0 || key > 255) {
        printf("Error: Key must be an integer between 0 and 255.\n");
        return 1;
    }

    char default_output[1024];
    const char* output_path = NULL;

    if (argc >= 4) {
        output_path = argv[3];
    } else {
        const char* last_slash = strrchr(input_path, '/');
        const char* last_backslash = strrchr(input_path, '\\');
        const char* last_separator = (last_slash > last_backslash) ? last_slash : last_backslash;
        const char* last_dot = strrchr(input_path, '.');
        
        if (last_dot != NULL && (last_separator == NULL || last_dot > last_separator)) {
            size_t base_len = last_dot - input_path;
            snprintf(default_output, sizeof(default_output), "%.*s.kog", (int)base_len, input_path);
        } else {
            snprintf(default_output, sizeof(default_output), "%s.kog", input_path);
        }
        output_path = default_output;
    }

    FILE* fin = fopen(input_path, "rb");
    if (!fin) {
        printf("Error: Cannot open input file %s\n", input_path);
        return 1;
    }

    FILE* fout = fopen(output_path, "wb");
    if (!fout) {
        printf("Error: Cannot open output file %s\n", output_path);
        fclose(fin);
        return 1;
    }

    fseek(fin, 0, SEEK_END);
    long file_size = ftell(fin);
    fseek(fin, 0, SEEK_SET);

    unsigned char buffer[4096];
    size_t bytes_read;
    uint32_t state = 0x9E3779B9u ^ (uint32_t)key;

    if (file_size >= 11) {
        unsigned char header[4];
        fread(header, 1, 4, fin);
        
        if (memcmp(header, "KOGX", 4) == 0) {
            unsigned char tail[7];
            fseek(fin, file_size - 7, SEEK_SET);
            fread(tail, 1, 7, fin);
            if (memcmp(tail, "FELONIA", 7) != 0) {
                printf("Error: Invalid KOGX file. Missing FELONIA suffix.\n");
                fclose(fin);
                fclose(fout);
                return 1;
            }
            
            printf("Detected KOGX+FELONIA. Decrypting file...\n");
            fseek(fin, 4, SEEK_SET); // Skip KOGX
            long payload_left = file_size - 11;
            
            while (payload_left > 0) {
                long to_read = payload_left < (long)sizeof(buffer) ? payload_left : (long)sizeof(buffer);
                bytes_read = fread(buffer, 1, to_read, fin);
                if (bytes_read == 0) break;
                
                for (size_t i = 0; i < bytes_read; ++i) {
                    state ^= state << 13;
                    state ^= state >> 17;
                    state ^= state << 5;
                    buffer[i] ^= (unsigned char)state;
                }
                fwrite(buffer, 1, bytes_read, fout);
                payload_left -= bytes_read;
            }
            
            fclose(fin);
            fclose(fout);
            printf("Success: %s generated (Decrypted, XOR Key: %d).\n", output_path, key);
            return 0;
        }
    }
    
    // Encrypt mode
    printf("Encrypting file...\n");
    fseek(fin, 0, SEEK_SET);
    fwrite("KOGX", 1, 4, fout);
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fin)) > 0) {
        for (size_t i = 0; i < bytes_read; ++i) {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            buffer[i] ^= (unsigned char)state;
        }
        fwrite(buffer, 1, bytes_read, fout);
    }
    fwrite("FELONIA", 1, 7, fout);

    fclose(fin);
    fclose(fout);

    printf("Success: %s generated (Encrypted, XOR Key: %d).\n", output_path, key);
    return 0;
}
