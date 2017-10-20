#ifndef SPI_FLASH_H
#define SPI_FLASH_H

#include <Arduino.h>
#include <SPI.h>

#define WB_WRITE_ENABLE       0x06
#define WB_WRITE_DISABLE      0x04
#define WB_CHIP_ERASE         0xc7
#define WB_READ_STATUS_REG_1  0x05
#define WB_READ_DATA          0x03
#define WB_PAGE_PROGRAM       0x02
#define WB_JEDEC_ID           0x9f

#define CS                    16

void SPIFlash_init(void);

void not_busy(void);

void read_page(unsigned int page_number, uint8_t *page_buffer);

void chip_erase(void);

void write_page(unsigned int page_number, uint8_t *page_buffer);

#endif