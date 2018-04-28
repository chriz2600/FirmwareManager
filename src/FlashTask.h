#include <global.h>
#include <Task.h>

extern int totalLength;
extern int readLength;
extern unsigned int page;
extern int prevPercentComplete;
extern bool finished;
extern MD5Builder md5;
extern File flashFile;
extern SPIFlash flash;

void reverseBitOrder(uint8_t *buffer);
void initBuffer(uint8_t *buffer);
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
        uint8_t buffer[256];

        virtual bool OnStart() {
            page = 0;
            finished = false;
            totalLength = -1;
            readLength = -1;
            prevPercentComplete = -1;

            md5.begin();
            flashFile = SPIFFS.open(FIRMWARE_FILE, "r");

            if (flashFile) {
                uint8_t buffer[256];
                totalLength = flashFile.size() / 256;

                flash.enable();
                flash.chip_erase_async();
                
                return true;
            } else {
                return false;
            }
        }

        virtual void OnStop() {
            flash.disable();
            flashFile.close();
            md5.calculate();
            String md5sum = md5.toString();
            _writeFile("/etc/last_flash_md5", md5sum.c_str(), md5sum.length());
            finished = true;
        }

        virtual void OnUpdate(uint32_t deltaTime) {
            int _read = 0;
            if (!flash.is_busy_async()) {
                if ((_read = flashFile.readBytes((char *) buffer, 256)) && page < PAGES) {
                    md5.add(buffer, _read);
                    reverseBitOrder(buffer);
                    flash.page_write_async(page, buffer);
                    initBuffer(buffer); // cleanup buffer
                    page++;
                } else {
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
};
