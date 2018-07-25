#include <inttypes.h>
#include <string.h>
#include "global.h"

#define MAX_ADDR_SPACE 128

int setDisplayOSD(uint8_t value) {
    brzo_i2c_start_transaction(FPGA_I2C_ADDR, FPGA_I2C_FREQ_KHZ);
    uint8_t buffer[2];
    buffer[0] = 0x81;
    buffer[1] = value;
    brzo_i2c_write(buffer, 2, false);
    return brzo_i2c_end_transaction();
}

int writeToOSD(uint8_t column, uint8_t row, const uint8_t *charData) {
    if (column > 39) { column = 39; }
    if (row > 23) { row = 23; }

    uint16_t localAddress = row * 40 + column;
    uint8_t upperAddress;
    uint8_t lowerAddress;
    uint8_t buffer[128];
    uint8_t len = strlen((char*) charData);
    uint8_t left = len;
    uint8_t towrite;

    brzo_i2c_start_transaction(FPGA_I2C_ADDR, FPGA_I2C_FREQ_KHZ);

    while (left > 0) {
        upperAddress = localAddress / MAX_ADDR_SPACE;
        lowerAddress = localAddress % MAX_ADDR_SPACE;
        buffer[0] = 0x80;
        buffer[1] = upperAddress;
        brzo_i2c_write(buffer, 2, false);
        buffer[0] = lowerAddress;
        towrite = MAX_ADDR_SPACE - lowerAddress;
        if (towrite > left) { towrite = left; }
        memcpy(&buffer[1], &charData[len-left], towrite);
        brzo_i2c_write(buffer, towrite + 1, false);
        left -= towrite;
        localAddress += towrite;
    }

    return brzo_i2c_end_transaction();
}
