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

void SPIFlash_not_busy(void);

void SPIFlash_page_read(unsigned int page_number, uint8_t *page_buffer);

void SPIFlash_chip_erase(void);

void SPIFlash_page_write(unsigned int page_number, uint8_t *page_buffer);

void SPIFlash_cs_enable(void);

void SPIFlash_cs_disable(void);

#endif