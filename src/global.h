#ifndef GLOBAL_H
#define GLOBAL_H

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

#define I2C_OSD_ADDR_OFFSET (0x80)
#define I2C_OSD_ENABLE (0x81)
#define I2C_OSD_ACTIVE_LINE (0x82)
#define I2C_OUTPUT_RESOLUTION (0x83)

// // controller data, int16
// /*
//     15: a, 14: b, 13: x, 12: y, 11: up, 10: down, 09: left, 08: right
//     07: start, 06: ltrigger, 05: rtrigger, 04: trigger_osd
// */
#define CTRLR_BUTTON_A (15)
#define CTRLR_BUTTON_B (14)
#define CTRLR_BUTTON_X (13)
#define CTRLR_BUTTON_Y (12)
#define CTRLR_PAD_UP (11)
#define CTRLR_PAD_DOWN (10)
#define CTRLR_PAD_LEFT (9)
#define CTRLR_PAD_RIGHT (8)
#define CTRLR_BUTTON_START (7)
#define CTRLR_LTRIGGER (6)
#define CTRLR_RTRIGGER (5)
#define CTRLR_TRIGGER_OSD (4)

#endif
