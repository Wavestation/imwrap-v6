#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    unsigned char buffer[4096];
    size_t bytes_read;
    unsigned char k = (unsigned char)key;

    // Check for KOGX header
    unsigned char header[4];
    bytes_read = fread(header, 1, 4, fin);
    int is_encrypted = 0;
    if (bytes_read == 4 && memcmp(header, "KOGX", 4) == 0) {
        is_encrypted = 1;
        printf("Detected KOGX header. Decrypting file...\n");
        // We just skip writing the KOGX header to fout
    } else {
        printf("No KOGX header detected. Encrypting file...\n");
        // Write KOGX header to output
        fwrite("KOGX", 1, 4, fout);
        // Process the 4 bytes we already read
        for (size_t i = 0; i < bytes_read; ++i) {
            header[i] ^= k;
        }
        fwrite(header, 1, bytes_read, fout);
    }

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fin)) > 0) {
        for (size_t i = 0; i < bytes_read; ++i) {
            buffer[i] ^= k;
        }
        size_t bytes_written = fwrite(buffer, 1, bytes_read, fout);
        if (bytes_written != bytes_read) {
            printf("Error: Failed to write to output file\n");
            fclose(fin);
            fclose(fout);
            return 1;
        }
    }

    fclose(fin);
    fclose(fout);

    printf("Success: %s generated (XOR Key: %d).\n", output_path, key);
    return 0;
}
