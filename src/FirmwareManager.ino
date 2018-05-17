/* 
    Dreamcast Firmware Manager
*/
#include "global.h"
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <SPIFlash.h>
#include "AsyncJson.h"
#include "ArduinoJson.h"
#include <Task.h>
#include "FlashTask.h"

char ssid[64];
char password[64];
char otaPassword[64]; 
char firmwareServer[1024] = "dc.i74.de";
char firmwareVersion[64] = "master";
char firmwareFPGA[64] = "10CL025";
char firmwareFormat[64] = "VGA";
char httpAuthUser[64] = "Test";
char httpAuthPass[64] = "testtest";
const char* host = "dc-firmware-manager";
const char* WiFiAPPSK = "geheim1234";
IPAddress ipAddress( 192, 168, 4, 1 );
bool inInitialSetupMode = false;
String fname;
AsyncWebServer server(80);
SPIFlash flash(CS);
int last_error = 0; 

static AsyncClient *aClient = NULL;
File flashFile;
bool headerFound = false;
String header = String();

unsigned int page;
bool finished;
int totalLength;
int readLength;
int prevPercentComplete;
MD5Builder md5;
TaskManager taskManager;
TaskFlash taskFlash(1);

void _writeFile(const char *filename, const char *towrite, unsigned int len) {
    File f = SPIFFS.open(filename, "w");
    if (f) {
        f.write((const uint8_t*) towrite, len);
        f.close();
        DBG_OUTPUT_PORT.printf(">> _writeFile: %s:[%s]\n", filename, towrite);
    }
}

void _readFile(const char *filename, char *target, unsigned int len) {
    bool exists = SPIFFS.exists(filename);
    if (exists) {
        File f = SPIFFS.open(filename, "r");
        if (f) {
            f.readString().toCharArray(target, len);
            f.close();
            DBG_OUTPUT_PORT.printf(">> _readFile: %s:[%s]\n", filename, target);
        }
    }
}

void setupCredentials(void) {
    DBG_OUTPUT_PORT.printf(">> Reading stored values...\n");

    _readFile("/etc/ssid", ssid, 64);
    _readFile("/etc/password", password, 64);
    _readFile("/etc/ota_pass", otaPassword, 64);
    _readFile("/etc/firmware_server", firmwareServer, 1024);
    _readFile("/etc/firmware_version", firmwareVersion, 64);
    _readFile("/etc/firmware_fpga", firmwareFPGA, 64);
    _readFile("/etc/firmware_format", firmwareFormat, 64);
    _readFile("/etc/http_auth_user", httpAuthUser, 64);
    _readFile("/etc/http_auth_pass", httpAuthPass, 64);

    if (DEBUG) {
        DBG_OUTPUT_PORT.printf("+---------------------------------------------------------------------\n");
        DBG_OUTPUT_PORT.printf("| /etc/ssid             -> ssid:            [%s]\n", ssid);
        DBG_OUTPUT_PORT.printf("| /etc/password         -> password:        [%s]\n", password);
        DBG_OUTPUT_PORT.printf("| /etc/ota_pass         -> otaPassword:     [%s]\n", otaPassword);
        DBG_OUTPUT_PORT.printf("| /etc/firmware_server  -> firmwareServer:  [%s]\n", firmwareServer);
        DBG_OUTPUT_PORT.printf("| /etc/firmware_version -> firmwareVersion: [%s]\n", firmwareVersion);
        DBG_OUTPUT_PORT.printf("| /etc/firmware_fpga    -> firmwareFPGA:    [%s]\n", firmwareFPGA);
        DBG_OUTPUT_PORT.printf("| /etc/firmware_format  -> firmwareFormat:  [%s]\n", firmwareFormat);
        DBG_OUTPUT_PORT.printf("| /etc/http_auth_user   -> httpAuthUser:    [%s]\n", httpAuthUser);
        DBG_OUTPUT_PORT.printf("| /etc/http_auth_pass   -> httpAuthPass:    [%s]\n", httpAuthPass);
        DBG_OUTPUT_PORT.printf("+---------------------------------------------------------------------\n");
    }
}

void setupAPMode(void) {
    WiFi.mode(WIFI_AP);
    uint8_t mac[WL_MAC_ADDR_LENGTH];
    WiFi.softAPmacAddress(mac);
    String macID = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) +
    String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
    macID.toUpperCase();
    String AP_NameString = String(host) + String("-") + macID;
    
    char AP_NameChar[AP_NameString.length() + 1];
    memset(AP_NameChar, 0, AP_NameString.length() + 1);
    
    for (int i=0; i<AP_NameString.length(); i++)
    AP_NameChar[i] = AP_NameString.charAt(i);
    
    WiFi.softAP(AP_NameChar, WiFiAPPSK);
    DBG_OUTPUT_PORT.printf(">> SSID:   %s\n", AP_NameChar);
    DBG_OUTPUT_PORT.printf(">> AP-PSK: %s\n", WiFiAPPSK);
    inInitialSetupMode = true;
}

void startFPGAConfiguration() {
    pinMode(NCONFIG, OUTPUT);
    digitalWrite(NCONFIG, LOW);
}

void endFPGAConfiguration() {
    digitalWrite(NCONFIG, HIGH);
    pinMode(NCONFIG, INPUT);    
}

void resetFPGAConfiguration() {
    startFPGAConfiguration();
    delay(1);
    endFPGAConfiguration();
}

bool _isAuthenticated(AsyncWebServerRequest *request) {
    return request->authenticate(httpAuthUser, httpAuthPass);
}

void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if(!_isAuthenticated(request)) {
        return;
    }
    if (!index) {
        fname = filename.c_str();
        md5.begin();
        request->_tempFile = SPIFFS.open(filename, "w");
        DBG_OUTPUT_PORT.printf(">> Receiving %s\n", filename.c_str());
    }
    if (request->_tempFile) {
        if (len) {
            request->_tempFile.write(data, len);
            md5.add(data, len);
        }
        if (final) {
            DBG_OUTPUT_PORT.printf(">> MD5 calc for %s\n", fname.c_str());
            request->_tempFile.close();
            md5.calculate();
            String md5sum = md5.toString();
            _writeFile((fname + ".md5").c_str(), md5sum.c_str(), md5sum.length());
        }
    }
}

int writeProgress(uint8_t *buffer, size_t maxLen, int progress) {
    char msg[5];
    int len = 4;
    int alen = (len > maxLen ? maxLen : len);

    sprintf(msg, "% 3i\n", progress);
    //len = strlen(msg);
    memcpy(buffer, msg, alen);
    return alen;
}

void handleFlash(AsyncWebServerRequest *request, const char *filename) {
    if (SPIFFS.exists(filename)) {
        taskManager.StartTask(&taskFlash);
        request->send(200);
    } else {
        request->send(404);
    }
}

/*
void handleFlashOld(AsyncWebServerRequest *request, const char *filename) {
    page = 0;
    finished = false;
    totalLength = -1;
    readLength = -1;
    md5.begin();
    flashFile = SPIFFS.open(filename, "r");

    if (flashFile) {
        totalLength = flashFile.size() / 256;

        flash.enable();
        flash.chip_erase_async();
    
        AsyncWebServerResponse *response = request->beginChunkedResponse(
            "text/plain", [filename](uint8_t *buffer, size_t maxLen, size_t index) -> size_t 
        {
            if (finished) {
                return 0;
            }

            int _read = 0;
            if (!flash.is_busy_async()) {
                if ((_read = flashFile.readBytes((char *) buffer, 256)) && page < PAGES) {
                    md5.add(buffer, _read);
                    reverseBitOrder(buffer);
                    flash.page_write_async(page, buffer);
                    initBuffer(buffer); // cleanup buffer
                    page++;
                } else {
                    flash.disable();
                    flashFile.close();
                    md5.calculate();
                    String md5sum = md5.toString();
                    _writeFile("/etc/last_flash_md5", md5sum.c_str(), md5sum.length());
                    finished = true;
                }
            }
            readLength = page;
            int percentComplete = (totalLength <= 0 ? 0 : (int)(readLength * 100 / totalLength));
            if (prevPercentComplete != percentComplete) {
                prevPercentComplete = percentComplete;
                DBG_OUTPUT_PORT.printf("[%i]\n", percentComplete);
            }
            return writeProgress(buffer, maxLen, percentComplete);
        });

        response->addHeader("Server", "Dreamcast HDMI");
        request->send(response);
    } else {
        request->send(404);
    }
}
*/

void handleDownload(AsyncWebServerRequest *request) {
    headerFound = false;
    header = String();
    totalLength = -1;
    readLength = -1;
    md5.begin();
    flashFile = SPIFFS.open(FIRMWARE_FILE, "w");

    if (flashFile) {
        aClient = new AsyncClient();

        aClient->onError([](void *arg, AsyncClient *client, int error) {
            DBG_OUTPUT_PORT.println("Connect Error");
            aClient = NULL;
            delete client;
        }, NULL);
    
        aClient->onConnect([](void *arg, AsyncClient *client) {
            DBG_OUTPUT_PORT.println("Connected");
            aClient->onError(NULL, NULL);

            client->onDisconnect([](void *arg, AsyncClient *c) {
                DBG_OUTPUT_PORT.println("onDisconnect");
                flashFile.close();
                md5.calculate();
                String md5sum = md5.toString();
                _writeFile((String(FIRMWARE_FILE) + ".md5").c_str(), md5sum.c_str(), md5sum.length());
                DBG_OUTPUT_PORT.println("Disconnected");
                aClient = NULL;
                delete c;
            }, NULL);
        
            client->onData([](void *arg, AsyncClient *c, void *data, size_t len) {
                uint8_t* d = (uint8_t*) data;

                if (!headerFound) {
                    String sData = String((char*) data);
                    int idx = sData.indexOf("\r\n\r\n");
                    if (idx == -1) {
                        DBG_OUTPUT_PORT.printf("header not found. Storing buffer.\n");
                        header += sData;
                        return;
                    } else {
                        header += sData.substring(0, idx + 4);
                        header.toLowerCase();
                        int clstart = header.indexOf("content-length: ");
                        if (clstart != -1) {
                            clstart += 16;
                            int clend = header.indexOf("\r\n", clstart);
                            if (clend != -1) {
                                totalLength = atoi(header.substring(clstart, clend).c_str());
                            }
                        }
                        d = (uint8_t*) sData.substring(idx + 4).c_str();
                        len = (len - (idx + 4));
                        headerFound = true;
                        readLength = 0;
                        DBG_OUTPUT_PORT.printf("header content length found: %i\n", totalLength);
                    }
                }
                readLength += len;
                DBG_OUTPUT_PORT.printf("write: %i, %i/%i\n", len, readLength, totalLength);
                flashFile.write(d, len);
                md5.add(d, len);
            }, NULL);
        
            //send the request
            DBG_OUTPUT_PORT.println("Requesting firmware...");

            String httpGet = "GET /fw/" + String(firmwareVersion) + "/DCxPlus-" + String(firmwareFPGA) + "-" + String(firmwareFormat) + "." + FIRMWARE_EXTENSION + " HTTP/1.0\r\nHost: dc.i74.de\r\n\r\n";
            client->write(httpGet.c_str());
        }, NULL);

        DBG_OUTPUT_PORT.println("Trying to connect");
        if (!aClient->connect(firmwareServer, 80)) {
            DBG_OUTPUT_PORT.println("Connect Fail");
            AsyncClient *client = aClient;
            aClient = NULL;
            delete client;
        }

        request->send(200);
    } else {
        request->send(500);
    }
}

void setupArduinoOTA() {
    DBG_OUTPUT_PORT.printf(">> Setting up ArduinoOTA...\n");
    
    ArduinoOTA.setPort(8266);
    ArduinoOTA.setHostname(host);
    ArduinoOTA.setPassword(otaPassword);
    
    ArduinoOTA.onStart([]() {
        DBG_OUTPUT_PORT.println("ArduinoOTA >> Start");
    });
    ArduinoOTA.onEnd([]() {
        DBG_OUTPUT_PORT.println("\nArduinoOTA >> End");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        DBG_OUTPUT_PORT.printf("ArduinoOTA >> Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        DBG_OUTPUT_PORT.printf("ArduinoOTA >> Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) {
            DBG_OUTPUT_PORT.println("ArduinoOTA >> Auth Failed");
        } else if (error == OTA_BEGIN_ERROR) {
            DBG_OUTPUT_PORT.println("ArduinoOTA >> Begin Failed");
        } else if (error == OTA_CONNECT_ERROR) {
            DBG_OUTPUT_PORT.println("ArduinoOTA >> Connect Failed");
        } else if (error == OTA_RECEIVE_ERROR) {
            DBG_OUTPUT_PORT.println("ArduinoOTA >> Receive Failed");
        } else if (error == OTA_END_ERROR) {
            DBG_OUTPUT_PORT.println("ArduinoOTA >> End Failed");
        }
    });
    ArduinoOTA.begin();
}

void setupWiFi() {
    //WIFI INIT
    WiFi.softAPdisconnect(true);
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);
    WiFi.mode(WIFI_STA);
    
    DBG_OUTPUT_PORT.printf(">> WiFi.getAutoConnect: %i\n", WiFi.getAutoConnect());
    DBG_OUTPUT_PORT.printf(">> Connecting to %s\n", ssid);

    if (String(WiFi.SSID()) != String(ssid)) {
        WiFi.begin(ssid, password);
        DBG_OUTPUT_PORT.printf(">> WiFi.begin: %s@%s\n", password, ssid);
    }
    
    bool success = true;
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        DBG_OUTPUT_PORT.print(".");
        if (tries == 60) {
            WiFi.disconnect();
            success = false;
            break;
        }
        tries++;
    }

    DBG_OUTPUT_PORT.printf(">> success: %i\n", success);

    if (!success) {
        // setup AP mode to configure ssid and password
        setupAPMode();
    } else {
        ipAddress = WiFi.localIP();
        DBG_OUTPUT_PORT.printf(
            ">> Connected! IP address: %d.%d.%d.%d\n", 
            ipAddress[0], ipAddress[1], ipAddress[2], ipAddress[3]
        );
    }
    
    if (MDNS.begin(host, ipAddress)) {
        DBG_OUTPUT_PORT.println(">> mDNS started");
        MDNS.addService("http", "tcp", 80);
        DBG_OUTPUT_PORT.printf(
            ">> http://%s.local/\n", 
            host
        );
    }
}

void writeSetupParameter(AsyncWebServerRequest *request, const char* param, char* target, unsigned int maxlen) {
    if(request->hasParam(param, true)) {
        String _tmp = "/etc/" + String(param);
        const char* filename = _tmp.c_str();
        AsyncWebParameter *p = request->getParam(param, true);
        if (p->value() == "") {
            DBG_OUTPUT_PORT.printf("SPIFFS.remove: %s\n", filename);
            SPIFFS.remove(filename);
        } else {
            snprintf(target, maxlen, "%s", p->value().c_str());
            _writeFile(filename, target, maxlen);
        }
    } else {
        DBG_OUTPUT_PORT.printf("no such param: %s\n", param);
    }
}

String jsonSafe(const char* in) {

}

void setupHTTPServer() {
    DBG_OUTPUT_PORT.printf(">> Setting up HTTP server...\n");

    server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        request->send(200);
    }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        handleUpload(request, FIRMWARE_FILE, index, data, len, final);
    });

    server.on("/upload-index", HTTP_POST, [](AsyncWebServerRequest *request){
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        request->send(200);
    }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        handleUpload(request, "/index.html.gz", index, data, len, final);
    });

    server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request){
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        handleDownload(request);
    });

    server.on("/flash", HTTP_GET, [](AsyncWebServerRequest *request){
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        handleFlash(request, FIRMWARE_FILE);
    });

    server.on("/cleanup", HTTP_GET, [](AsyncWebServerRequest *request) {
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        SPIFFS.remove(FIRMWARE_FILE);
        request->send(200);
    });
/*
    server.on("/secureflash", HTTP_GET, [](AsyncWebServerRequest *request){
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        //startFPGAConfiguration();
        handleFlashOld(request, FIRMWARE_FILE);
        //endFPGAConfiguration();
    });
*/

    server.on("/progress", HTTP_GET, [](AsyncWebServerRequest *request) {
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        DBG_OUTPUT_PORT.printf("progress requested...\n");
        char msg[64];
        if (last_error) {
            sprintf(msg, "ERROR %i\n", last_error);
            request->send(200, "text/plain", msg);
            DBG_OUTPUT_PORT.printf("...delivered: %s (%i).\n", msg, last_error);
            // clear last_error
            last_error = 0;
        } else {
            sprintf(msg, "%i\n", totalLength <= 0 ? 0 : (int)(readLength * 100 / totalLength));
            request->send(200, "text/plain", msg);
            DBG_OUTPUT_PORT.printf("...delivered: %s.\n", msg);
        }
    });

    server.on("/flash_size", HTTP_GET, [](AsyncWebServerRequest *request) {
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        char msg[64];
        sprintf(msg, "%lu\n", ESP.getFlashChipSize());
        request->send(200, "text/plain", msg);
    });

    server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request) {
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        DBG_OUTPUT_PORT.printf("FPGA reset requested...\n");
        resetFPGAConfiguration();
        request->send(200, "text/plain", "OK\n");
        DBG_OUTPUT_PORT.printf("...delivered.\n");
    });

    server.on("/issetupmode", HTTP_GET, [](AsyncWebServerRequest *request) {
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        request->send(200, "text/plain", inInitialSetupMode ? "true\n" : "false\n");
    });

    server.on("/setup", HTTP_POST, [](AsyncWebServerRequest *request) {
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        writeSetupParameter(request, "ssid", ssid, 64);
        writeSetupParameter(request, "password", password, 64);
        writeSetupParameter(request, "ota_pass", otaPassword, 64);
        writeSetupParameter(request, "firmware_server", firmwareServer, 1024);
        writeSetupParameter(request, "firmware_version", firmwareVersion, 64);
        writeSetupParameter(request, "firmware_fpga", firmwareFPGA, 64);
        writeSetupParameter(request, "firmware_format", firmwareFormat, 64);
        writeSetupParameter(request, "http_auth_user", httpAuthUser, 64);
        writeSetupParameter(request, "http_auth_pass", httpAuthPass, 64);

        request->send(200, "text/plain", "OK\n");
    });
    
    server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        
        AsyncResponseStream *response = request->beginResponseStream("text/json");
        DynamicJsonBuffer jsonBuffer;
        JsonObject &root = jsonBuffer.createObject();

        root["ssid"] = ssid;
        root["password"] = password;
        root["ota_pass"] = otaPassword;
        root["firmware_server"] = firmwareServer;
        root["firmware_version"] = firmwareVersion;
        root["firmware_fpga"] = firmwareFPGA;
        root["firmware_format"] = firmwareFormat;
        root["http_auth_user"] = httpAuthUser;
        root["http_auth_pass"] = httpAuthPass;
        
        root.printTo(*response);
        request->send(response);
    });

    server.on("/restart", HTTP_ANY, [](AsyncWebServerRequest *request) {
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        ESP.eraseConfig();
        ESP.restart();
    });

    AsyncStaticWebHandler* handler = &server
        .serveStatic("/", SPIFFS, "/")
        .setDefaultFile("index.html");
    // set authentication by configured user/pass later
    handler->setAuthentication(httpAuthUser, httpAuthPass);

    server.begin();
}

void setupSPIFFS() {
    DBG_OUTPUT_PORT.printf(">> Setting up SPIFFS...\n");
    SPIFFS.begin();
    {
        FSInfo fs_info;
        SPIFFS.info(fs_info);

        DBG_OUTPUT_PORT.printf(">> totalBytes: (%lu)\n", fs_info.totalBytes);
        DBG_OUTPUT_PORT.printf(">> usedBytes: (%lu)\n", fs_info.usedBytes);
        DBG_OUTPUT_PORT.printf(">> blockSize: (%lu)\n", fs_info.blockSize);
        DBG_OUTPUT_PORT.printf(">> pageSize: (%lu)\n", fs_info.pageSize);
        DBG_OUTPUT_PORT.printf(">> maxOpenFiles: (%lu)\n", fs_info.maxOpenFiles);
        DBG_OUTPUT_PORT.printf(">> maxPathLength: (%lu)\n", fs_info.maxPathLength);
        DBG_OUTPUT_PORT.printf(">> maxSketchSpace 1: (%lu)\n", ESP.getFreeSketchSpace());
        DBG_OUTPUT_PORT.printf(">> maxSketchSpace 2: (%lu)\n", (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000);
        DBG_OUTPUT_PORT.printf(">> flashChipSize: (%lu)\n", ESP.getFlashChipSize());

        Dir dir = SPIFFS.openDir("/");
        while (dir.next()) {
            String fileName = dir.fileName();
            size_t fileSize = dir.fileSize();
            DBG_OUTPUT_PORT.printf(">> %s (%lu)\n", fileName.c_str(), fileSize);
        }
        DBG_OUTPUT_PORT.printf("\n");
    }
}

void setupTaskManager() {
    DBG_OUTPUT_PORT.printf(">> Setting up task manager...\n");
    taskManager.Setup();
}

void setup(void) {

    DBG_OUTPUT_PORT.begin(115200);
    DBG_OUTPUT_PORT.printf("\n>> FirmwareManager starting...\n");
    DBG_OUTPUT_PORT.setDebugOutput(DEBUG);

    pinMode(NCONFIG, INPUT);    

    setupSPIFFS();
    setupCredentials();
    setupWiFi();
    setupHTTPServer();
    
    if (strlen(otaPassword)) 
    {
        setupArduinoOTA();
    }

    setupTaskManager();
    DBG_OUTPUT_PORT.println(">> Ready.");
}

void loop(void){
    ArduinoOTA.handle();
    taskManager.Loop();
}
