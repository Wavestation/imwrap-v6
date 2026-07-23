#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage: kogxor <input_file> <key_0_to_255> [output_file]\n");
        printf("Example: kogxor MT32_CONTROL.ROM 42\n");
        return 1;
    }

    const char* input_path = argv[1];
    int key = atoi(argv[2]);

    if (key < 0 || key > 255) {
        printf("Error: Key must be an integer between 0 and 255.\n");
        return 1;
    }

    char default_output[1024];
    const char* output_path = NULL;

    if (argc >= 4) {
        output_path = argv[3];
    } else {
        snprintf(default_output, sizeof(default_output), "%s.kog", input_path);
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
