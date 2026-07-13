#include <stdint.h>
#include <stdio.h>

int main(void) {
    uint8_t mbr[512] = {0};
    FILE *f = fopen("disk.img", "wb");

    if (!f)
        return 1;

    // First primary partition
    // FAT32 LBA at LBA 1
    mbr[446] = 0x00;
    mbr[450] = 0x0C;
    mbr[454] = 0x01;
    mbr[458] = 0xFF;
    mbr[459] = 0xFF;
    // MBR signature.
    mbr[510] = 0x55;
    mbr[511] = 0xAA;

    fwrite(mbr, 1, sizeof(mbr), f);

    for (int i = 0; i < 32 * 1024 * 1024 - (int)sizeof(mbr); i++)
        fputc(0, f);

    fclose(f);
    return 0;
}
