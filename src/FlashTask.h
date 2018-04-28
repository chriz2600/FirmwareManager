#include <global.h>
#include <Task.h>
#include <fastlz.h>

#define BLOCK_SIZE 1536

extern int totalLength;
extern int readLength;
extern unsigned int page;
extern int prevPercentComplete;
extern bool finished;
extern MD5Builder md5;
extern File flashFile;
extern SPIFlash flash;

void reverseBitOrder(uint8_t *buffer);
void _writeFile(const char *filename, const char *towrite, unsigned int len);
void _readFile(const char *filename, char *target, unsigned int len);

extern TaskManager taskManager;

class TaskFlash : public Task {

    public:
        TaskFlash(uint8_t v) :
            Task(1),
            dummy(v)
        { };

    private:
        uint8_t dummy;

        uint8_t *buffer = NULL;
        uint8_t *result = NULL;
        uint8_t *result_start = NULL;

        size_t bytes_in_result = 0;
        int chunk_size = 0;

        virtual bool OnStart() {
            page = 0;
            finished = false;
            totalLength = -1;
            readLength = 0;
            prevPercentComplete = -1;
            // local
            chunk_size = 0;
            bytes_in_result = 0;

            buffer = (uint8_t *) malloc(BLOCK_SIZE);
            result = (uint8_t *) malloc(BLOCK_SIZE);
            result_start = result;
            // initialze complete buffer with flash "default"
            initBuffer(result, BLOCK_SIZE);

            md5.begin();
            flashFile = SPIFFS.open(FIRMWARE_FILE, "r");

            if (flashFile) {
                flashFile.readBytes((char *) buffer, 4);
                md5.add(buffer, 4);
                totalLength = (buffer[0] + (buffer[1] << 8) + (buffer[2] << 16) + (buffer[3] << 24)) / 256;

                flash.enable();
                flash.chip_erase_async();
                
                return true;
            } else {
                return false;
            }
        }

        virtual void OnUpdate(uint32_t deltaTime) {
            int _read = 0;
            if (!flash.is_busy_async()) {
                if (page >= PAGES || doFlash() == -1) {
                    taskManager.StopTask(this);
                }
            }
            readLength = page;
            int percentComplete = (totalLength <= 0 ? 0 : (int)(readLength * 100 / totalLength));
            if (prevPercentComplete != percentComplete) {
                prevPercentComplete = percentComplete;
                DBG_OUTPUT_PORT.printf("[%i]\n", percentComplete);
            }
        }

        void initBuffer(uint8_t *buffer, int len) {
            for (int i = 0 ; i < len ; i++) {
                // flash default (unwritten) is 0xff
                buffer[i] = 0xff; 
            }
        }

        int doFlash() {
            int bytes_write = 0;

            if (chunk_size > 0) {
                // step 3: decompress
                bytes_in_result = fastlz_decompress(buffer, chunk_size, result, BLOCK_SIZE);
                chunk_size = 0;
                return 0; /* no bytes written yet */
            }

            if (bytes_in_result == 0) {
                // reset the result buffer pointer
                result = result_start;
                // step 1: read chunk header
                if (flashFile.readBytes((char *) buffer, 2) == 0) {
                    // no more chunks
                    return -1;
                }
                md5.add(buffer, 2);
                chunk_size = buffer[0] + (buffer[1] << 8);
                // step 2: read chunk length from file
                flashFile.readBytes((char *) buffer, chunk_size);
                md5.add(buffer, chunk_size);
                return 0; /* no bytes written yet */
            }

            // step 4: write data to flash, in 256 byte long pages
            bytes_write = bytes_in_result > 256 ? 256 : bytes_in_result;
            /* 
                even it's called async, it actually writes 256 bytes over SPI bus, it's just not calling the blocking "busy wait"
            */
            flash.page_write_async(page, result);
            /* 
                cleanup last 256 byte area afterwards, as spi flash memory is always written in chunks of 256 byte
            */
            initBuffer(result, 256);
            bytes_in_result -= bytes_write;
            result += bytes_write; /* advance the char* by written bytes */
            page++;
            return bytes_write; /* 256 bytes written */
        }

        virtual void OnStop() {
            flash.disable();
            flashFile.close();
            md5.calculate();
            String md5sum = md5.toString();
            _writeFile("/etc/last_flash_md5", md5sum.c_str(), md5sum.length());
            finished = true;
            free(buffer);
            // make sure pointer is reset to original start
            result = result_start;
            free(result);
            buffer = NULL;
            result = NULL;
            result_start = NULL;
        }
};
