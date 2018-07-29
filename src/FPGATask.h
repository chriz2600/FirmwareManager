#include <global.h>
#include <Task.h>
#include <inttypes.h>
#include <string.h>
#include <brzo_i2c.h>

#define MAX_ADDR_SPACE 128

typedef std::function<void(uint8_t address, const uint8_t *buffer, uint8_t len)> FPGAEventHandlerFunction;

void setupI2C() {
    DBG_OUTPUT_PORT.printf(">> Setting up I2C master...\n");
    brzo_i2c_setup(FPGA_I2C_SDA, FPGA_I2C_SCL, CLOCK_STRETCH_TIMEOUT);
}

extern TaskManager taskManager;

class FPGATask : public Task {

    public:
        FPGATask(uint8_t repeat, FPGAEventHandlerFunction chandler) :
            Task(repeat),
            controller_handler(chandler)
        { };

        virtual void DoWriteToOSD(uint8_t column, uint8_t row, uint8_t charData[]) { 
            if (column > 39) { column = 39; }
            if (row > 23) { row = 23; }

            len = strlen((char*) charData);
            localAddress = row * 40 + column;
            left = len;
            data_in = (uint8_t*) malloc(len);
            memcpy(data_in, charData, len);
            updateOSDContent = true;
        }

        virtual void ActivateDisplayOSD(uint8_t value) {
            OSDActivationValue = value;
            updateOSDActivation = true;
        }

    private:
        FPGAEventHandlerFunction controller_handler;

        uint8_t *data_in;

        uint8_t data_out[128];
        uint8_t data_write[128];
        uint8_t len;
        uint16_t localAddress;
        uint8_t upperAddress;
        uint8_t lowerAddress;
        uint8_t towrite;
        uint8_t left;
        bool updateOSDContent;
        bool updateOSDActivation;

        uint8_t OSDActivationValue;

        virtual bool OnStart() {
            return true;
        }

        virtual void OnUpdate(uint32_t deltaTime) {
            brzo_i2c_start_transaction(FPGA_I2C_ADDR, FPGA_I2C_FREQ_KHZ);
            if (updateOSDContent) {
                if (left > 0) {
                    upperAddress = localAddress / MAX_ADDR_SPACE;
                    lowerAddress = localAddress % MAX_ADDR_SPACE;
                    data_write[0] = 0x80;
                    data_write[1] = upperAddress;
                    brzo_i2c_write(data_write, 2, false);
                    data_write[0] = lowerAddress;
                    towrite = MAX_ADDR_SPACE - lowerAddress;
                    if (towrite > left) { towrite = left; }
                    memcpy(&data_write[1], &data_in[len-left], towrite);
                    brzo_i2c_write(data_write, towrite + 1, false);
                    left -= towrite;
                    localAddress += towrite;
                } else {
                    updateOSDContent = false;
                }
            } else if (updateOSDActivation) {
                uint8_t buffer[2];
                buffer[0] = 0x81;
                buffer[1] = OSDActivationValue;
                brzo_i2c_write(buffer, 2, false);
                updateOSDActivation = false;
            } else {
                // update controller data
                uint8_t buffer[1];
                uint8_t buffer2[2];
                buffer[0] = 0x85;
                brzo_i2c_write(buffer, 1, false);
                brzo_i2c_read(buffer2, 2, false);
                if (buffer2[0] != data_out[0] || buffer2[1] != data_out[1]) {
                    controller_handler(0x85, buffer2, 2);
                }
                memcpy(data_out, buffer2, 2);
            }
            if (brzo_i2c_end_transaction()) {
                last_error = ERROR_WRONG_VERSION;
            }
        }

        virtual void OnStop() {
            if (data_in != NULL) {
                free(data_in);
            }
        }
};
