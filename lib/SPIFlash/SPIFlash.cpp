#include <SPIFlash.h>

void SPIFlash_not_busy(void) {
    digitalWrite(CS, HIGH);  
    digitalWrite(CS, LOW);
    SPI.transfer(WB_READ_STATUS_REG_1);       
    while (SPI.transfer(0) & 1) {
        yield();
    }; 
    digitalWrite(CS, HIGH);  
}

void SPIFlash_page_read(unsigned int page_number, uint8_t *page_buffer) {
    digitalWrite(CS, HIGH);
    digitalWrite(CS, LOW);
    SPI.transfer(WB_READ_DATA);
    // Construct the 24-bit address from the 16-bit page
    // number and 0x00, since we will read 256 bytes (one
    // page).
    SPI.transfer((page_number >> 8) & 0xFF);
    SPI.transfer((page_number >> 0) & 0xFF);
    SPI.transfer(0);
    for (int i = 0; i < 256; ++i) {
        page_buffer[i] = SPI.transfer(0);
        yield();
    }
    digitalWrite(CS, HIGH);
    SPIFlash_not_busy();
}

void SPIFlash_chip_erase(void) {
    digitalWrite(CS, HIGH);
    digitalWrite(CS, LOW);  
    SPI.transfer(WB_WRITE_ENABLE);
    digitalWrite(CS, HIGH);
    digitalWrite(CS, LOW);
    SPI.transfer(WB_CHIP_ERASE);
    digitalWrite(CS, HIGH);
    SPIFlash_not_busy();
}

void SPIFlash_init(void) {
    SPI.begin();
    SPI.setDataMode(0);
    SPI.setBitOrder(MSBFIRST);
    SPI.setFrequency(32000000);
    // initialze to not bother with CS
    pinMode(CS, INPUT);
}

void SPIFlash_page_write(unsigned int page_number, uint8_t *page_buffer) {
    digitalWrite(CS, HIGH);
    digitalWrite(CS, LOW);  
    SPI.transfer(WB_WRITE_ENABLE);
    digitalWrite(CS, HIGH);
    digitalWrite(CS, LOW);  
    SPI.transfer(WB_PAGE_PROGRAM);
    SPI.transfer((page_number >>  8) & 0xFF);
    SPI.transfer((page_number >>  0) & 0xFF);
    SPI.transfer(0);
    for (int i = 0; i < 256; ++i) {
      SPI.transfer(page_buffer[i]);
    }
    digitalWrite(CS, HIGH);
    SPIFlash_not_busy();
}
  
void SPIFlash_cs_enable(void) {
    // activate CS
    pinMode(CS, OUTPUT);
}

void SPIFlash_cs_disable(void) {
    // disable CS
    pinMode(CS, INPUT);
}
