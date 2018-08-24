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
#include "FlashESPTask.h"
#include "FlashESPIndexTask.h"
#include "FPGATask.h"
#include "Menu.h"

#define DEFAULT_SSID ""
#define DEFAULT_PASSWORD ""
#define DEFAULT_OTA_PASSWORD ""
#define DEFAULT_FW_SERVER "dc.i74.de"
#define DEFAULT_FW_VERSION "master"
#define DEFAULT_HTTP_USER "Test"
#define DEFAULT_HTTP_PASS "testtest"
#define DEFAULT_CONF_IP_ADDR ""
#define DEFAULT_CONF_IP_GATEWAY ""
#define DEFAULT_CONF_IP_MASK ""
#define DEFAULT_CONF_IP_DNS ""
#define DEFAULT_HOST "dc-firmware-manager"
#define DEFAULT_FORCEVGA "128"
#define DEFAULT_RESOLUTION "0"

char ssid[64] = DEFAULT_SSID;
char password[64] = DEFAULT_PASSWORD;
char otaPassword[64] = DEFAULT_OTA_PASSWORD; 
char firmwareServer[1024] = DEFAULT_FW_SERVER;
char firmwareVersion[64] = DEFAULT_FW_VERSION;
char httpAuthUser[64] = DEFAULT_HTTP_USER;
char httpAuthPass[64] = DEFAULT_HTTP_PASS;
char confIPAddr[24] = DEFAULT_CONF_IP_ADDR;
char confIPGateway[24] = DEFAULT_CONF_IP_GATEWAY;
char confIPMask[24] = DEFAULT_CONF_IP_MASK;
char confIPDNS[24] = DEFAULT_CONF_IP_DNS;
char host[64] = DEFAULT_HOST;
char forceVGA[8] = "";
char configuredResolution[8] = "";
const char* WiFiAPPSK = "geheim1234";
IPAddress ipAddress( 192, 168, 4, 1 );
bool inInitialSetupMode = false;
bool fpgaDisabled = false;
String fname;
AsyncWebServer server(80);
SPIFlash flash(CS);
int last_error = NO_ERROR; 
int totalLength;
int readLength;

static AsyncClient *aClient = NULL;
File flashFile;
bool headerFound = false;
String header = String();

bool OSDOpen = false;
uint8_t OSDCurrentResolution = RESOLUTION_1080p;
uint8_t OSDForceVGA = VGA_ON;
uint8_t menu_activeLine = 255;
MD5Builder md5;
TaskManager taskManager;
FlashTask flashTask(1);
FlashESPTask flashESPTask(1);
FlashESPIndexTask flashESPIndexTask(1);

extern Menu mainMenu;
Menu *currentMenu;
// functions
void setOSD(bool value, WriteCallbackHandlerFunction handler);

void openOSD() {
    currentMenu = &mainMenu;
    setOSD(true, [](uint8_t Address, uint8_t Value) {
        currentMenu->Display();
    });
}

void closeOSD() {
    setOSD(false, NULL);
}

void waitForI2CRecover();

Menu outputResMenu("OutputResMenu", (uint8_t*) OSD_OUTPUT_RES_MENU, 11, 14, [](uint16_t controller_data) {
    if (CHECK_MASK(controller_data, CTRLR_BUTTON_B)) {
        currentMenu = &mainMenu;
        currentMenu->Display();
        return;
    }
    if (CHECK_MASK(controller_data, CTRLR_BUTTON_A)) {
        uint8_t value = RESOLUTION_1080p;

        switch (menu_activeLine) {
            case 11:
                value = RESOLUTION_VGA;
                break;
            case 12:
                value = RESOLUTION_480p;
                break;
            case 13:
                value = RESOLUTION_960p;
                break;
            case 14:
                value = RESOLUTION_1080p;
                break;
        }

        bool valueChanged = (value != OSDCurrentResolution);
        OSDCurrentResolution = value;
        DBG_OUTPUT_PORT.printf("setting output resolution: %u\n", (OSDForceVGA | OSDCurrentResolution));
        fpgaTask.Write(I2C_OUTPUT_RESOLUTION, OSDForceVGA | OSDCurrentResolution, [valueChanged](uint8_t Address, uint8_t Value) {
            DBG_OUTPUT_PORT.printf("switch resolution callback: %u\n", Value);
            if (valueChanged) {
                waitForI2CRecover();
            }
            DBG_OUTPUT_PORT.printf("Turn FOLLOWUP save menu on!\n");
            // TODO: Go to save output settings menu!
        });
        return;
    }
}, [](uint8_t address, uint8_t value) {
    DBG_OUTPUT_PORT.printf("Here in output res menu display callback!\n");
    menu_activeLine = 14 - OSDCurrentResolution;
    fpgaTask.Write(I2C_OSD_ACTIVE_LINE, menu_activeLine);
    // char buffer[521] = OSD_OUTPUT_RES_MENU;
    // buffer[menu_activeLine*40] = ">";
    // currentMenu->SetMenuText((uint8_t*) buffer);
    //currentMenu->Display(false, NULL);
});

Menu debugMenu("DebugMenu", (uint8_t*) OSD_DEBUG_MENU, 255, 255, [](uint16_t controller_data) {
    if (CHECK_MASK(controller_data, CTRLR_BUTTON_B)) {
        currentMenu = &mainMenu;
        currentMenu->Display();
        return;
    }
}, NULL);

Menu mainMenu("MainMenu", (uint8_t*) OSD_MAIN_MENU, 11, 12, [](uint16_t controller_data) {
    if (CHECK_MASK(controller_data, CTRLR_BUTTON_B)) {
        closeOSD();
        return;
    }
    if (CHECK_MASK(controller_data, CTRLR_BUTTON_A)) {
        switch (menu_activeLine) {
            case 11:
                currentMenu = &outputResMenu;
                currentMenu->Display();
                break;
            case 12:
                currentMenu = &debugMenu;
                currentMenu->Display();
                break;
        }
        return;
    }
}, NULL);

FPGATask fpgaTask(1, [](uint16_t controller_data) {
    if (!OSDOpen && CHECK_BIT(controller_data, CTRLR_TRIGGER_OSD)) {
        openOSD();
        return;
    }
    if (OSDOpen) {
        DBG_OUTPUT_PORT.printf("Menu: %s %x\n", currentMenu->Name(), controller_data);
        currentMenu->HandleClick(controller_data);
    }
});

void waitForI2CRecover() {
    int retryCount = 2600;
    int prev_last_error = NO_ERROR;
    DBG_OUTPUT_PORT.printf("... PRE: prev_last_error/last_error %i (%u/%u)\n", retryCount, prev_last_error, last_error);
    while (retryCount >= 0) {
        fpgaTask.Read(I2C_PING, 1, NULL); 
        fpgaTask.ForceLoop();
        if (prev_last_error != NO_ERROR && last_error == NO_ERROR) {
            break;
        }
        prev_last_error = last_error;
        retryCount--;
        delayMicroseconds(200);
        yield();
    }
    DBG_OUTPUT_PORT.printf("... POST: prev_last_error/last_error %i (%u/%u)\n", retryCount, prev_last_error, last_error);
}

void switchResolution(uint8_t newValue) {
    OSDCurrentResolution = newValue;
    fpgaTask.Write(I2C_OUTPUT_RESOLUTION, OSDForceVGA | OSDCurrentResolution, NULL);
}

void setOSD(bool value, WriteCallbackHandlerFunction handler) {
    OSDOpen = value;
    fpgaTask.Write(I2C_OSD_ENABLE, value, handler);
}

void _writeFile(const char *filename, const char *towrite, unsigned int len) {
    File f = SPIFFS.open(filename, "w");
    if (f) {
        f.write((const uint8_t*) towrite, len);
        f.close();
        DBG_OUTPUT_PORT.printf(">> _writeFile: %s:[%s]\n", filename, towrite);
    }
}

void _readFile(const char *filename, char *target, unsigned int len, const char* defaultValue) {
    bool exists = SPIFFS.exists(filename);
    bool readFromFile = false;
    if (exists) {
        File f = SPIFFS.open(filename, "r");
        if (f) {
            f.readString().toCharArray(target, len);
            f.close();
            DBG_OUTPUT_PORT.printf(">> _readFile: %s:[%s]\n", filename, target);
            readFromFile = true;
        }
    }
    if (!readFromFile) {
        snprintf(target, len, "%s", defaultValue);
        DBG_OUTPUT_PORT.printf(">> _readFile: %s:[%s] (default)\n", filename, target);
    }
}

void setupCredentials(void) {
    DBG_OUTPUT_PORT.printf(">> Reading stored values...\n");

    _readFile("/etc/ssid", ssid, 64, DEFAULT_SSID);
    _readFile("/etc/password", password, 64, DEFAULT_PASSWORD);
    _readFile("/etc/ota_pass", otaPassword, 64, DEFAULT_OTA_PASSWORD);
    _readFile("/etc/firmware_server", firmwareServer, 1024, DEFAULT_FW_SERVER);
    _readFile("/etc/firmware_version", firmwareVersion, 64, DEFAULT_FW_VERSION);
    _readFile("/etc/http_auth_user", httpAuthUser, 64, DEFAULT_HTTP_USER);
    _readFile("/etc/http_auth_pass", httpAuthPass, 64, DEFAULT_HTTP_PASS);
    _readFile("/etc/conf_ip_addr", confIPAddr, 24, DEFAULT_CONF_IP_ADDR);
    _readFile("/etc/conf_ip_gateway", confIPGateway, 24, DEFAULT_CONF_IP_GATEWAY);
    _readFile("/etc/conf_ip_mask", confIPMask, 24, DEFAULT_CONF_IP_MASK);
    _readFile("/etc/conf_ip_dns", confIPDNS, 24, DEFAULT_CONF_IP_DNS);
    _readFile("/etc/hostname", host, 64, DEFAULT_HOST);

    // if (DEBUG) {
    //     DBG_OUTPUT_PORT.printf("+---------------------------------------------------------------------\n");
    //     DBG_OUTPUT_PORT.printf("| /etc/ssid             -> ssid:            [%s]\n", ssid);
    //     DBG_OUTPUT_PORT.printf("| /etc/password         -> password:        [%s]\n", password);
    //     DBG_OUTPUT_PORT.printf("| /etc/ota_pass         -> otaPassword:     [%s]\n", otaPassword);
    //     DBG_OUTPUT_PORT.printf("| /etc/firmware_server  -> firmwareServer:  [%s]\n", firmwareServer);
    //     DBG_OUTPUT_PORT.printf("| /etc/firmware_version -> firmwareVersion: [%s]\n", firmwareVersion);
    //     DBG_OUTPUT_PORT.printf("| /etc/http_auth_user   -> httpAuthUser:    [%s]\n", httpAuthUser);
    //     DBG_OUTPUT_PORT.printf("| /etc/http_auth_pass   -> httpAuthPass:    [%s]\n", httpAuthPass);
    //     DBG_OUTPUT_PORT.printf("| /etc/conf_ip_addr     -> confIPAddr:      [%s]\n", confIPAddr);
    //     DBG_OUTPUT_PORT.printf("| /etc/conf_ip_gateway  -> confIPGateway:   [%s]\n", confIPGateway);
    //     DBG_OUTPUT_PORT.printf("| /etc/conf_ip_mask     -> confIPMask:      [%s]\n", confIPMask);
    //     DBG_OUTPUT_PORT.printf("| /etc/conf_ip_dns      -> confIPDNS:       [%s]\n", confIPDNS);
    //     DBG_OUTPUT_PORT.printf("| /etc/hostname         -> host:            [%s]\n", host);
    //     DBG_OUTPUT_PORT.printf("+---------------------------------------------------------------------\n");
    // }
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
    
    for (uint i=0; i<AP_NameString.length(); i++) {
        AP_NameChar[i] = AP_NameString.charAt(i);
    }

    WiFi.softAP(AP_NameChar, WiFiAPPSK);
    DBG_OUTPUT_PORT.printf(">> SSID:   %s\n", AP_NameChar);
    DBG_OUTPUT_PORT.printf(">> AP-PSK: %s\n", WiFiAPPSK);
    inInitialSetupMode = true;
}

void disableFPGA() {
    pinMode(NCE, OUTPUT);
    digitalWrite(NCE, HIGH);
    fpgaDisabled = true;
}

void enableFPGA() {
    if (fpgaDisabled) {
        digitalWrite(NCE, LOW);
        pinMode(NCE, INPUT);
        fpgaDisabled = false;
    }
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
    uint len = 4;
    int alen = (len > maxLen ? maxLen : len);

    sprintf(msg, "% 3i\n", progress);
    //len = strlen(msg);
    memcpy(buffer, msg, alen);
    return alen;
}

void resetall() {
    DBG_OUTPUT_PORT.printf("all reset requested...\n");
    enableFPGA();
    resetFPGAConfiguration();
    ESP.eraseConfig();
    ESP.restart();
}

void handleFlash(AsyncWebServerRequest *request, const char *filename) {
    if (SPIFFS.exists(filename)) {
        taskManager.StartTask(&flashTask);
        request->send(200);
    } else {
        request->send(404);
    }
}

void handleESPFlash(AsyncWebServerRequest *request, const char *filename) {
    if (SPIFFS.exists(filename)) {
        taskManager.StartTask(&flashESPTask);
        request->send(200);
    } else {
        request->send(404);
    }
}

void handleESPIndexFlash(AsyncWebServerRequest *request) {
    if (SPIFFS.exists(ESP_INDEX_STAGING_FILE)) {
        taskManager.StartTask(&flashESPIndexTask);
        request->send(200);
    } else {
        request->send(404);
    }
}

void handleFPGADownload(AsyncWebServerRequest *request) {
    String httpGet = "GET /fw/" 
        + String(firmwareVersion) 
        + "/DCxPlus-default"
        + "." + FIRMWARE_EXTENSION 
        + " HTTP/1.0\r\nHost: dc.i74.de\r\n\r\n";

    _handleDownload(request, FIRMWARE_FILE, httpGet);
}

void handleESPDownload(AsyncWebServerRequest *request) {
    String httpGet = "GET /" 
        + String(firmwareVersion) 
        + "/" + (ESP.getFlashChipSize() / 1024 / 1024) + "MB"
        + "-" + "firmware"
        + "." + ESP_FIRMWARE_EXTENSION 
        + " HTTP/1.0\r\nHost: esp.i74.de\r\n\r\n";

    _handleDownload(request, ESP_FIRMWARE_FILE, httpGet);
}

void handleESPIndexDownload(AsyncWebServerRequest *request) {
    String httpGet = "GET /"
        + String(firmwareVersion)
        + "/esp.index.html.gz"
        + " HTTP/1.0\r\nHost: esp.i74.de\r\n\r\n";

    _handleDownload(request, ESP_INDEX_STAGING_FILE, httpGet);
}

void _handleDownload(AsyncWebServerRequest *request, const char *filename, String httpGet) {
    headerFound = false;
    header = String();
    totalLength = -1;
    readLength = -1;
    last_error = NO_ERROR;
    md5.begin();
    flashFile = SPIFFS.open(filename, "w");

    if (flashFile) {
        aClient = new AsyncClient();

        aClient->onError([](void *arg, AsyncClient *client, int error) {
            DBG_OUTPUT_PORT.println("Connect Error");
            aClient = NULL;
            delete client;
        }, NULL);
    
        aClient->onConnect([ filename, httpGet ](void *arg, AsyncClient *client) {
            DBG_OUTPUT_PORT.println("Connected");
            aClient->onError(NULL, NULL);

            client->onDisconnect([ filename ](void *arg, AsyncClient *c) {
                DBG_OUTPUT_PORT.println("onDisconnect");
                flashFile.close();
                md5.calculate();
                String md5sum = md5.toString();
                _writeFile((String(filename) + ".md5").c_str(), md5sum.c_str(), md5sum.length());
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
    bool doStaticIpConfig = false;
    IPAddress ipAddr;
    doStaticIpConfig = ipAddr.fromString(confIPAddr);
    IPAddress ipGateway;
    doStaticIpConfig = doStaticIpConfig && ipGateway.fromString(confIPGateway);
    IPAddress ipMask;
    doStaticIpConfig = doStaticIpConfig && ipMask.fromString(confIPMask);
    IPAddress ipDNS;
    doStaticIpConfig = doStaticIpConfig && ipDNS.fromString(confIPDNS);

    //WIFI INIT
    WiFi.disconnect();
    WiFi.softAPdisconnect(true);
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);
    WiFi.hostname(host);

    DBG_OUTPUT_PORT.printf(">> Do static ip configuration: %i\n", doStaticIpConfig);
    if (doStaticIpConfig) {
        WiFi.config(ipAddr, ipGateway, ipMask, ipDNS);
    }

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
        IPAddress gateway = WiFi.gatewayIP();
        IPAddress subnet = WiFi.subnetMask();

        DBG_OUTPUT_PORT.printf(
            ">> Connected!\n   IP address:      %d.%d.%d.%d\n",
            ipAddress[0], ipAddress[1], ipAddress[2], ipAddress[3]
        );
        DBG_OUTPUT_PORT.printf(
            "   Gateway address: %d.%d.%d.%d\n",
            gateway[0], gateway[1], gateway[2], gateway[3]
        );
        DBG_OUTPUT_PORT.printf(
            "   Subnet mask:     %d.%d.%d.%d\n",
            subnet[0], subnet[1], subnet[2], subnet[3]
        );
        DBG_OUTPUT_PORT.printf(
            "   Hostname:        %s\n",
            WiFi.hostname().c_str()
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

void writeSetupParameter(AsyncWebServerRequest *request, const char* param, char* target, unsigned int maxlen, const char* resetValue) {
    if(request->hasParam(param, true)) {
        String _tmp = "/etc/" + String(param);
        const char* filename = _tmp.c_str();
        AsyncWebParameter *p = request->getParam(param, true);
        if (p->value() == "") {
            DBG_OUTPUT_PORT.printf("SPIFFS.remove: %s\n", filename);
            snprintf(target, maxlen, "%s", resetValue);
            SPIFFS.remove(filename);
        } else {
            snprintf(target, maxlen, "%s", p->value().c_str());
            _writeFile(filename, target, maxlen);
        }
    } else {
        DBG_OUTPUT_PORT.printf("no such param: %s\n", param);
    }
}

void setupHTTPServer() {
    DBG_OUTPUT_PORT.printf(">> Setting up HTTP server...\n");

    server.on("/upload/fpga", HTTP_POST, [](AsyncWebServerRequest *request){
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        request->send(200);
    }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        handleUpload(request, FIRMWARE_FILE, index, data, len, final);
    });

    server.on("/upload/esp", HTTP_POST, [](AsyncWebServerRequest *request){
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        request->send(200);
    }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        handleUpload(request, ESP_FIRMWARE_FILE, index, data, len, final);
    });

    server.on("/upload/index", HTTP_POST, [](AsyncWebServerRequest *request){
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        request->send(200);
    }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        handleUpload(request, ESP_INDEX_STAGING_FILE, index, data, len, final);
    });

    server.on("/list-files", HTTP_GET, [](AsyncWebServerRequest *request){
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }

        AsyncResponseStream *response = request->beginResponseStream("text/json");
        DynamicJsonBuffer jsonBuffer;
        JsonObject &root = jsonBuffer.createObject();

        FSInfo fs_info;
        SPIFFS.info(fs_info);

        root["totalBytes"] = fs_info.totalBytes;
        root["usedBytes"] = fs_info.usedBytes;
        root["blockSize"] = fs_info.blockSize;
        root["pageSize"] = fs_info.pageSize;
        root["maxOpenFiles"] = fs_info.maxOpenFiles;
        root["maxPathLength"] = fs_info.maxPathLength;
        root["freeSketchSpace"] = ESP.getFreeSketchSpace();
        root["maxSketchSpace"] = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        root["flashChipSize"] = ESP.getFlashChipSize();
        root["freeHeapSize"] = ESP.getFreeHeap();

        JsonArray &datas = root.createNestedArray("files");

        Dir dir = SPIFFS.openDir("/");
        while (dir.next()) {
        JsonObject &data = datas.createNestedObject();
            data["name"] = dir.fileName();
            data["size"] = dir.fileSize();
        }

        root.printTo(*response);
        request->send(response);
    });

    server.on("/download/fpga", HTTP_GET, [](AsyncWebServerRequest *request){
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        handleFPGADownload(request);
    });

    server.on("/download/esp", HTTP_GET, [](AsyncWebServerRequest *request){
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        handleESPDownload(request);
    });

    server.on("/download/index", HTTP_GET, [](AsyncWebServerRequest *request){
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        handleESPIndexDownload(request);
    });

    server.on("/flash/fpga", HTTP_GET, [](AsyncWebServerRequest *request){
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        handleFlash(request, FIRMWARE_FILE);
    });

    server.on("/flash/esp", HTTP_GET, [](AsyncWebServerRequest *request){
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        handleESPFlash(request, ESP_FIRMWARE_FILE);
    });

    server.on("/flash/index", HTTP_GET, [](AsyncWebServerRequest *request){
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        handleESPIndexFlash(request);
    });

    server.on("/cleanupindex", HTTP_GET, [](AsyncWebServerRequest *request) {
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        SPIFFS.remove(ESP_INDEX_FILE);
        SPIFFS.remove(String(ESP_INDEX_FILE) + ".md5");
        request->send(200);
    });

    server.on("/cleanup", HTTP_GET, [](AsyncWebServerRequest *request) {
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        SPIFFS.remove(FIRMWARE_FILE);
        SPIFFS.remove(ESP_FIRMWARE_FILE);
        SPIFFS.remove(ESP_INDEX_STAGING_FILE);
        SPIFFS.remove(String(FIRMWARE_FILE) + ".md5");
        SPIFFS.remove(String(ESP_FIRMWARE_FILE) + ".md5");
        SPIFFS.remove(String(ESP_INDEX_STAGING_FILE) + ".md5");
        // remove legacy config data
        SPIFFS.remove("/etc/firmware_fpga");
        SPIFFS.remove("/etc/firmware_format");
        request->send(200);
    });

    server.on("/flash/secure/fpga", HTTP_GET, [](AsyncWebServerRequest *request){
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        disableFPGA();
        handleFlash(request, FIRMWARE_FILE);
    });

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
            last_error = NO_ERROR;
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
        sprintf(msg, "%lu\n", (long unsigned int) ESP.getFlashChipSize());
        request->send(200, "text/plain", msg);
    });

    server.on("/reset/fpga", HTTP_GET, [](AsyncWebServerRequest *request) {
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        DBG_OUTPUT_PORT.printf("FPGA reset requested...\n");
        enableFPGA();
        resetFPGAConfiguration();
        request->send(200, "text/plain", "OK\n");
        DBG_OUTPUT_PORT.printf("...delivered.\n");
    });

    server.on("/reset/all", HTTP_GET, [](AsyncWebServerRequest *request) {
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        resetall();
    });

    server.on("/issetupmode", HTTP_GET, [](AsyncWebServerRequest *request) {
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        request->send(200, "text/plain", inInitialSetupMode ? "true\n" : "false\n");
    });

    server.on("/ping", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200);
    });

    server.on("/setup", HTTP_POST, [](AsyncWebServerRequest *request) {
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        writeSetupParameter(request, "ssid", ssid, 64, DEFAULT_SSID);
        writeSetupParameter(request, "password", password, 64, DEFAULT_PASSWORD);
        writeSetupParameter(request, "ota_pass", otaPassword, 64, DEFAULT_OTA_PASSWORD);
        writeSetupParameter(request, "firmware_server", firmwareServer, 1024, DEFAULT_FW_SERVER);
        writeSetupParameter(request, "firmware_version", firmwareVersion, 64, DEFAULT_FW_VERSION);
        writeSetupParameter(request, "http_auth_user", httpAuthUser, 64, DEFAULT_HTTP_USER);
        writeSetupParameter(request, "http_auth_pass", httpAuthPass, 64, DEFAULT_HTTP_PASS);
        writeSetupParameter(request, "conf_ip_addr", confIPAddr, 24, DEFAULT_CONF_IP_ADDR);
        writeSetupParameter(request, "conf_ip_gateway", confIPGateway, 24, DEFAULT_CONF_IP_GATEWAY);
        writeSetupParameter(request, "conf_ip_mask", confIPMask, 24, DEFAULT_CONF_IP_MASK);
        writeSetupParameter(request, "conf_ip_dns", confIPDNS, 24, DEFAULT_CONF_IP_DNS);
        writeSetupParameter(request, "hostname", host, 64, DEFAULT_HOST);

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
        root["http_auth_user"] = httpAuthUser;
        root["http_auth_pass"] = httpAuthPass;
        root["flash_chip_size"] = ESP.getFlashChipSize();
        root["conf_ip_addr"] = confIPAddr;
        root["conf_ip_gateway"] = confIPGateway;
        root["conf_ip_mask"] = confIPMask;
        root["conf_ip_dns"] = confIPDNS;
        root["hostname"] = host;
        root["fw_version"] = FW_VERSION;

        root.printTo(*response);
        request->send(response);
    });

    server.on("/reset/esp", HTTP_ANY, [](AsyncWebServerRequest *request) {
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        ESP.eraseConfig();
        ESP.restart();
    });

    server.on("/test", HTTP_GET, [](AsyncWebServerRequest *request) {
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
    });

    server.on("/osdwrite", HTTP_POST, [](AsyncWebServerRequest *request) {
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }

        AsyncWebParameter *column = request->getParam("column", true);
        AsyncWebParameter *row = request->getParam("row", true);
        AsyncWebParameter *text = request->getParam("text", true);

        fpgaTask.DoWriteToOSD(atoi(column->value().c_str()), atoi(row->value().c_str()), (uint8_t*) text->value().c_str());
        request->send(200);
    });

    server.on("/osd/on", HTTP_GET, [](AsyncWebServerRequest *request) {
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        openOSD();
        request->send(200);
    });

    server.on("/osd/off", HTTP_GET, [](AsyncWebServerRequest *request) {
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        closeOSD();
        request->send(200);
    });

    server.on("/res/VGA", HTTP_GET, [](AsyncWebServerRequest *request) {
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        switchResolution(RESOLUTION_VGA);
        request->send(200);
    });

    server.on("/res/480p", HTTP_GET, [](AsyncWebServerRequest *request) {
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        switchResolution(RESOLUTION_480p);
        request->send(200);
    });

    server.on("/res/960p", HTTP_GET, [](AsyncWebServerRequest *request) {
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        switchResolution(RESOLUTION_960p);
        request->send(200);
    });

    server.on("/res/1080p", HTTP_GET, [](AsyncWebServerRequest *request) {
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        switchResolution(RESOLUTION_1080p);
        request->send(200);
    });

    server.on("/reset/pll", HTTP_GET, [](AsyncWebServerRequest *request) {
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        fpgaTask.Write(I2C_OUTPUT_RESOLUTION, OSDForceVGA | OSDCurrentResolution | PLL_RESET_ON, NULL);
        request->send(200);
    });

    server.on("/power/down/hdmi", HTTP_GET, [](AsyncWebServerRequest *request) {
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        fpgaTask.Write(I2C_POWER, HDMI_POWER_DOWN, NULL);
        request->send(200);
    });

    AsyncStaticWebHandler* handler = &server
        .serveStatic("/", SPIFFS, "/")
        .setDefaultFile("index.html");
    // set authentication by configured user/pass later
    handler->setAuthentication(httpAuthUser, httpAuthPass);

    server.onNotFound([](AsyncWebServerRequest *request){
        if (request->url().endsWith(".md5")) {
           request->send(200, "text/plain", "00000000000000000000000000000000\n");
           return;
        }
        request->send(404);
    });

    server.begin();
}

void setupSPIFFS() {
    DBG_OUTPUT_PORT.printf(">> Setting up SPIFFS...\n");
    if (!SPIFFS.begin()) {
        DBG_OUTPUT_PORT.printf(">> SPIFFS begin failed, trying to format...");
        if (SPIFFS.format()) {
            DBG_OUTPUT_PORT.printf("done.\n");
        } else {
            DBG_OUTPUT_PORT.printf("error.\n");
        }
    }
    // {
    //     FSInfo fs_info;
    //     SPIFFS.info(fs_info);

    //     DBG_OUTPUT_PORT.printf(">> totalBytes: (%u)\n", fs_info.totalBytes);
    //     DBG_OUTPUT_PORT.printf(">> usedBytes: (%u)\n", fs_info.usedBytes);
    //     DBG_OUTPUT_PORT.printf(">> blockSize: (%u)\n", fs_info.blockSize);
    //     DBG_OUTPUT_PORT.printf(">> pageSize: (%u)\n", fs_info.pageSize);
    //     DBG_OUTPUT_PORT.printf(">> maxOpenFiles: (%u)\n", fs_info.maxOpenFiles);
    //     DBG_OUTPUT_PORT.printf(">> maxPathLength: (%u)\n", fs_info.maxPathLength);
    //     DBG_OUTPUT_PORT.printf(">> maxSketchSpace 1: (%u)\n", ESP.getFreeSketchSpace());
    //     DBG_OUTPUT_PORT.printf(">> maxSketchSpace 2: (%u)\n", (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000);
    //     DBG_OUTPUT_PORT.printf(">> flashChipSize: (%u)\n", ESP.getFlashChipSize());
    //     DBG_OUTPUT_PORT.printf(">> freeHeapSize: (%u)\n", ESP.getFreeHeap());

    //     Dir dir = SPIFFS.openDir("/");
    //     while (dir.next()) {
    //         String fileName = dir.fileName();
    //         size_t fileSize = dir.fileSize();
    //         DBG_OUTPUT_PORT.printf(">> %s (%u)\n", fileName.c_str(), fileSize);
    //     }
    //     DBG_OUTPUT_PORT.printf("\n");
    // }
}

void setupTaskManager() {
    DBG_OUTPUT_PORT.printf(">> Setting up task manager...\n");
    taskManager.Setup();
    taskManager.StartTask(&fpgaTask);
}

void setupOutputResolution() {
    int retryCount = 500;
    int retries = 0;

    _readFile("/etc/force_vga", forceVGA, 8, DEFAULT_FORCEVGA);
    OSDForceVGA = atoi(forceVGA);

    _readFile("/etc/resolution", configuredResolution, 8, DEFAULT_RESOLUTION);
    OSDCurrentResolution = atoi(configuredResolution);

    DBG_OUTPUT_PORT.printf(">> Setting up output resolution: %x\n", OSDForceVGA | OSDCurrentResolution);
    while (retryCount >= 0) {
        retries++;
        fpgaTask.Write(I2C_OUTPUT_RESOLUTION, OSDForceVGA | OSDCurrentResolution, NULL);
        fpgaTask.ForceLoop();
        retryCount--;
        if (last_error == NO_ERROR) {
            break;
        }
        delayMicroseconds(5000);
        yield();
    }
    DBG_OUTPUT_PORT.printf("   retry loops needed: %i\n", retries);
}

void setup(void) {

    DBG_OUTPUT_PORT.begin(115200);
    DBG_OUTPUT_PORT.printf("\n>> FirmwareManager starting...\n");
    DBG_OUTPUT_PORT.setDebugOutput(DEBUG);

    pinMode(NCE, INPUT);    
    pinMode(NCONFIG, INPUT);

    setupI2C();
    setupSPIFFS();
    setupOutputResolution();
    setupTaskManager();
    setupCredentials();
    setupWiFi();
    setupHTTPServer();
    
    if (strlen(otaPassword)) 
    {
        setupArduinoOTA();
    }

    setOSD(false, NULL); fpgaTask.ForceLoop();
    DBG_OUTPUT_PORT.println(">> Ready.");
}

void loop(void){
    ArduinoOTA.handle();
    taskManager.Loop();
}
