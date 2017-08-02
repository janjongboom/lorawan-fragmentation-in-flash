#include <stdint.h>
#include <iostream>
#include <string>
#include "crc.h"

uint8_t buffer[5120];

int main() {
    FILE *fp = fopen("../test-file-no-padding.bin", "r");

    fread(buffer, sizeof(uint8_t), sizeof(buffer), fp);

    uint64_t crc = crc64(0, buffer, sizeof(buffer));
    printf("crc64 is %02llx\n", crc);

    fclose(fp);

    return 0;
}
