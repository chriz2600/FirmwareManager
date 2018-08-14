#define CS 16
#define NCE 4
#define NCONFIG 5
#define DBG_OUTPUT_PORT Serial

#define FPGA_I2C_ADDR 0x3c
#define FPGA_I2C_FREQ_KHZ 733
#define FPGA_I2C_SCL 0
#define FPGA_I2C_SDA 2
#define CLOCK_STRETCH_TIMEOUT 200

#define FIRMWARE_FILE "/firmware.dc"
#define FIRMWARE_EXTENSION "dc"

#define ESP_FIRMWARE_FILE "/firmware.bin"
#define ESP_FIRMWARE_EXTENSION "bin"
#define ESP_INDEX_FILE "/index.html.gz"
#define ESP_INDEX_STAGING_FILE "/esp.index.html.gz"

#define PAGES 8192 // 8192 pages x 256 bytes = 2MB = 16MBit
#define DEBUG true

#ifndef GLOBAL_H
#define GLOBAL_H

#define ERROR_WRONG_MAGIC 16
#define ERROR_WRONG_VERSION 17
#define ERROR_FILE 18
#define ERROR_FILE_SIZE 19
#define ERROR_END_I2C_TRANSACTION 20

#define FW_VERSION "__FW_VERSION__"

#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))

#define FPGA_UPDATE_OSD_DONE 0x00
#define FPGA_WRITE_DONE ((uint8_t) 0x01)

#define RESOLUTION_1080p (0x00)
#define RESOLUTION_960p (0x01)
#define RESOLUTION_480p (0x02)
#define RESOLUTION_VGA (0x03)

#endif
