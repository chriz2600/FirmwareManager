#include <global.h>
#include <Task.h>

extern MD5Builder md5;
extern int totalLength;
extern int readLength;
extern int last_error;

void _writeFile(const char *filename, const char *towrite, unsigned int len);
void _readFile(const char *filename, char *target, unsigned int len);

extern TaskManager taskManager;

#define BUFFER_SIZE 1024
#define DEBUG_UPDATER

class FlashESPIndexTask : public Task {

    public:
        FlashESPIndexTask(uint8_t v) :
            Task(1),
            dummy(v)
        { };

    private:
        uint8_t dummy;
        uint8_t buffer[BUFFER_SIZE];
        int prevPercentComplete;
        File sourceFile;
        File targetFile;

        virtual bool OnStart() {
            totalLength = -1;
            readLength = 0;
            prevPercentComplete = -1;
            
            md5.begin();
            sourceFile = SPIFFS.open(ESP_INDEX_STAGING_FILE, "r");
            targetFile = SPIFFS.open(ESP_INDEX_FILE, "w");

            if (sourceFile && targetFile) {
                totalLength = sourceFile.size();
                return true;
            } else {
                last_error = ERROR_FILE;
                return false;
            }
        }

        virtual void OnUpdate(uint32_t deltaTime) {
            int bytes_read = 0;

            if (sourceFile.available()) {
                bytes_read = sourceFile.readBytes((char *) buffer, BUFFER_SIZE);
                
                if (bytes_read == 0) {
                    taskManager.StopTask(this);
                } else {
                    md5.add(buffer, bytes_read);
                    targetFile.write(buffer, BUFFER_SIZE);
                }
            } else {
                taskManager.StopTask(this);
            }
            
            readLength += bytes_read;
            int percentComplete = (totalLength <= 0 ? 0 : (int)(readLength * 100 / totalLength));
            if (prevPercentComplete != percentComplete) {
                prevPercentComplete = percentComplete;
                DBG_OUTPUT_PORT.printf("[%i]\n", percentComplete);
            }
        }

        virtual void OnStop() {
            sourceFile.close();
            targetFile.close();
            md5.calculate();
            String md5sum = md5.toString();
            _writeFile("/index.html.gz.md5", md5sum.c_str(), md5sum.length());
            DBG_OUTPUT_PORT.printf("2: flashing ESP index finished.\n");
        }
};
