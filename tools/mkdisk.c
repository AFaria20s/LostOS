#include <stdio.h>

int main() {
    FILE *f = fopen("disk.img", "wb");
    char zero = 0;
    
    // 32MB of zeros
    for (int i = 0; i < 32 * 1024 * 1024; i++)
        fwrite(&zero, 1, 1, f);
    
    // assign MBR in bytes 510-511
    fseek(f, 510, SEEK_SET);
    fputc(0x55, f);
    fputc(0xAA, f);
    
    fclose(f);
    return 0;
}