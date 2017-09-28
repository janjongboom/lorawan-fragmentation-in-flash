#include <stdint.h>
#include <iostream>
#include <string>
#include "crc.h"

int main(int argc, char** argv) {
    FILE *fp = fopen(argv[1], "r");
    fseek(fp, 0L, SEEK_END);
    size_t sz = ftell(fp);

    uint8_t* buffer = (uint8_t*)malloc(sz);

    fseek(fp, 0L, SEEK_SET);

    fread(buffer, sizeof(uint8_t), sz, fp);

    uint64_t crc = crc64(0, buffer, sz);
    printf("%02llx", crc);

    free(buffer);

    fclose(fp);

    return 0;
}
