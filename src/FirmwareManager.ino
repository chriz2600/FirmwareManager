/* 
FSWebServer - Example WebServer with SPIFFS backend for esp8266
Copyright (c) 2015 Hristo Gochkov. All rights reserved.
This file is part of the ESP8266WebServer library for Arduino environment.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.
You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
//#include <ESP8266WebServer.h>
//#include <ESP8266HTTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <SPIFlash.h>

#define CS 16
#define NCONFIG 5
#define DBG_OUTPUT_PORT Serial
#define FIRMWARE_FILE "/firmware.rbf"
#define FIRMWARE_FILE_ORIG "/firmware.orig.rbf"
#define FIRMWARE_URL "http://dc.i74.de/firmware.rbf"

#define PAGES 8192 // 8192 pages x 256 bytes = 2MB = 16MBit
#define DEBUG true

char ssid[64];
char password[64];
char otaPassword[64]; 
char firmwareUrl[1024] = FIRMWARE_URL;
const char* host = "dc-firmware-manager";
const char* WiFiAPPSK = "geheim1234";
IPAddress ipAddress( 192, 168, 4, 1 );
bool inInitialSetupMode = false;
String fname;

const uint8_t empty_buffer[256] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

//ESP8266WebServer server(80);
AsyncWebServer server(80);
SPIFlash flash(CS);

static AsyncClient *aClient = NULL;
File downloadFile;
MD5Builder md5;

/*
String getContentType(String filename){
    if(server.hasArg("download")) return "application/octet-stream";
    else if(filename.endsWith(".htm")) return "text/html";
    else if(filename.endsWith(".html")) return "text/html";
    else if(filename.endsWith(".css")) return "text/css";
    else if(filename.endsWith(".js")) return "application/javascript";
    else if(filename.endsWith(".png")) return "image/png";
    else if(filename.endsWith(".gif")) return "image/gif";
    else if(filename.endsWith(".jpg")) return "image/jpeg";
    else if(filename.endsWith(".ico")) return "image/x-icon";
    else if(filename.endsWith(".xml")) return "text/xml";
    else if(filename.endsWith(".pdf")) return "application/x-pdf";
    else if(filename.endsWith(".zip")) return "application/x-zip";
    else if(filename.endsWith(".gz")) return "application/x-gzip";
    return "text/plain";
}*/

/*
bool handleFileRead(String path){
    if (path.endsWith("/")) {
        path += "index.html";
    }
    String contentType = getContentType(path);
    String pathWithGz = path + ".gz";
    if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {
        if (SPIFFS.exists(pathWithGz)) {
            path += ".gz";
        }
        File file = SPIFFS.open(path, "r");
        size_t sent = server.streamFile(file, contentType);
        file.close();
        return true;
    }
    return false;
}
*/
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

String _md5sum(File f) {
    if (f && f.seek(0, SeekSet)) {
        MD5Builder md5;
        md5.begin();
        md5.addStream(f, f.size());
        md5.calculate();
        // rewind back to start
        f.seek(0, SeekSet);
        return md5.toString();
    }
    return String();
}

void setupCredentials(void) {
    DBG_OUTPUT_PORT.printf(">> Reading stored values...\n");
    _readFile("/etc/ssid", ssid, 64);
    _readFile("/etc/password", password, 64);
    _readFile("/etc/ota_pass", otaPassword, 64);
    _readFile("/etc/firmware_url", firmwareUrl, 1024);
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
/*
bool copyBlockwise(String source, String destination, unsigned int length) {
    unsigned int i = 0;
    uint8_t buff[256];
    File src = SPIFFS.open(source, "r");
    if (src) {
        File dest = SPIFFS.open(destination, "w");
        if (dest) {
            server.sendContent("shorten file:\n");
            while (i <= length) {
                server.sendContent(String(i) + "\n");
                src.readBytes((char *) buff, 256);
                dest.write(buff, 256);
                i++;
            }
            dest.close();
        }
        src.close();
    }
}
*/
void initBuffer(uint8_t *buffer) {
    for (int i = 0 ; i < 256 ; i++) { 
        buffer[i] = 0xff; 
        yield(); 
    }
}
/*
void handleFileList() {
    if (!server.hasArg("dir")) {
        server.send(500, "text/plain", "BAD ARGS"); 
        return;
    }
    
    String path = server.arg("dir");
    DBG_OUTPUT_PORT.println(">> handleFileList: " + path);
    Dir dir = SPIFFS.openDir(path);
    path = String();
    
    String output = "[";
    while(dir.next()){
        File entry = dir.openFile("r");
        if (output != "[") output += ',';
        bool isDir = false;
        output += "{\"type\":\"";
        output += (isDir)?"dir":"file";
        output += "\",\"name\":\"";
        output += String(entry.name()).substring(1);
        output += "\",\"length\":\"";
        output += String(entry.size());
        output += "\"}";
        entry.close();
    }
    
    output += "]";
    server.send(200, "text/json", output);
}
*/
/*
void handleProgramFlash() {
    unsigned int page = 0;
    uint8_t buffer[256];
    // initialze
    initBuffer(buffer);
    
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    server.setContentLength(CONTENT_LENGTH_UNKNOWN); // *** BEGIN ***
    server.send(200, "text/plain", "");
    
    File f = SPIFFS.open(FIRMWARE_FILE, "r");
    int pagesToProgram = f.size() / 256;
    if (f) {
        String md5sum = _md5sum(f);
        flash.enable();
        flash.chip_erase();
        while (f.readBytes((char *) buffer, 256) && page < PAGES) {
            server.sendContent(String(page) + "/" + pagesToProgram + "\n");
            reverseBitOrder(buffer);
            flash.page_write(page, buffer);
            // cleanup buffer
            initBuffer(buffer);
            page++;
        }
        flash.disable();
        f.close();
        _writeFile("/etc/last_flash_md5", md5sum.c_str(), md5sum.length());
    }
    
    server.sendContent("FPGA reset...\n");
    resetFPGAConfiguration();
    server.sendContent("done\n");
    server.sendContent(""); // *** END 1/2 ***
    server.client().stop(); // *** END 2/2 ***
}
*/
void resetFPGAConfiguration() {
    pinMode(NCONFIG, OUTPUT);
    digitalWrite(NCONFIG, LOW);
    delay(10);
    digitalWrite(NCONFIG, HIGH);
    pinMode(NCONFIG, INPUT);    
}

void reverseBitOrder(uint8_t *buffer) {
    for (int i = 0 ; i < 256 ; i++) { 
        buffer[i] = (buffer[i] & 0xF0) >> 4 | (buffer[i] & 0x0F) << 4;
        buffer[i] = (buffer[i] & 0xCC) >> 2 | (buffer[i] & 0x33) << 2;
        buffer[i] = (buffer[i] & 0xAA) >> 1 | (buffer[i] & 0x55) << 1;
        yield(); 
    }
}
/*
void handleDumpFirmware() {
    uint8_t page_buffer[256];
    unsigned int last_page = 0;
    WiFiClient client = server.client();

    client.print("HTTP/1.1 200 OK\r\n");
    client.print("Content-Disposition: attachment; filename=flash.full.rbf\r\n");
    client.print("Content-Type: application/octet-stream\r\n");
    client.print("Content-Length: -1\r\n");
    client.print("Connection: close\r\n");
    client.print("Access-Control-Allow-Origin: *\r\n");
    client.print("\r\n");

    flash.enable();
    for (unsigned int i = 0; i < PAGES; ++i) {
        flash.page_read(i, page_buffer);
        reverseBitOrder(page_buffer);
        client.write((const char*) page_buffer, 256);
    }
    flash.disable();
    client.stop();
}
*/
/*
    curl -D - -F "file=@$PWD/output_file.rbf" "http://dc-firmware-manager.local/upload-firmware?size=368011"
void handleUploadFirmware(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    //Handle upload
    handleUpload(request, FIRMWARE_FILE, index, data, len, final);
}

void handleUploadIndexHtml(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    //Handle upload
    handleUpload(request, "/index.html.gz", index, data, len, final);
}
*/
bool _isAuthenticated(AsyncWebServerRequest *request) {
    return request->authenticate("Test", "testtest");
}

void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    if(!_isAuthenticated(request)) {
        return;
    }
    if (!index) {
        // normalize filename
        if (filename == "index.html.gz") filename = String("/index.html.gz");
        else filename = String(FIRMWARE_FILE);
        
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

bool headerFound;
String header;

void handleDownload(AsyncWebServerRequest *request) {
    header = String();
    headerFound = false;
    md5.begin();
    downloadFile = SPIFFS.open(FIRMWARE_FILE_ORIG, "w");

    if (downloadFile) {
        aClient = new AsyncClient();

        aClient->onError([](void *arg, AsyncClient *client, int error) {
            DBG_OUTPUT_PORT.println("Connect Error");
            aClient = NULL;
            delete client;
        }, NULL);
    
        aClient->onConnect([](void *arg, AsyncClient *client) {
            Serial.println("Connected");
            aClient->onError(NULL, NULL);

            client->onDisconnect([](void *arg, AsyncClient *c) {
                downloadFile.close();
                md5.calculate();
                String md5sum = md5.toString();
                _writeFile((String(FIRMWARE_FILE_ORIG) + ".md5").c_str(), md5sum.c_str(), md5sum.length());
                Serial.println("Disconnected");
                aClient = NULL;
                delete c;
            }, NULL);
        
            client->onData([](void *arg, AsyncClient *c, void *data, size_t len) {
                DBG_OUTPUT_PORT.printf("Data 1: %i\n", len);
                uint8_t* d = (uint8_t*) data;

                if (!headerFound) {
                    String sData = String((char*) data);
                    int idx = sData.indexOf("\r\n\r\n");
                    if (idx == -1) {
                        header += sData;
                        return;
                    } else {
                        header += sData.substring(0, idx + 4);
                        d = (uint8_t*) sData.substring(idx + 4).c_str();
                        len = (len - (idx + 4));
                        headerFound = true;
                    }
                }

                DBG_OUTPUT_PORT.printf("Data 2: %i\n", len);
                downloadFile.write(d, len);
                md5.add(d, len);
            }, NULL);
        
            //send the request
            client->write("GET /firmware.rbf HTTP/1.0\r\nHost: dc.i74.de\r\n\r\n");
            // Content-Length: 368011
        }, NULL);

        DBG_OUTPUT_PORT.println("Trying to connect");
        if(!aClient->connect("dc.i74.de", 80)) {
            DBG_OUTPUT_PORT.println("Connect Fail");
            AsyncClient *client = aClient;
            aClient = NULL;
            delete client;
        }

        AsyncWebServerResponse *response = request->beginChunkedResponse(
            "text/plain", [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t 
        {
            char msg[64];
            int len;
            //Write up to "maxLen" bytes into "buffer" and return the amount written.
            //index equals the amount of bytes that have been already sent
            //You will be asked for more data until 0 is returned
            //Keep in mind that you can not delay or yield waiting for more data!
            if (!aClient) {
                return 0;
            }

            DBG_OUTPUT_PORT.println("HERE");
            sprintf(msg, "...\n");
            len = strlen(msg);
            memcpy(buffer, msg, (len > maxLen ? maxLen : len));
            return len;
          });
          response->addHeader("Server", "ESP Async Web Server");
          request->send(response);
    } else {
        request->send(500);
    }
}

/*
void handleUploadFirmware() {
    handleUpload(FIRMWARE_FILE);
}

void handleUploadIndexHtml() {
    handleUpload("/index.html.gz");
}    

void handleUpload(const char* filename) {
    if (!_isAuthenticated()) {
        return;
    }

    HTTPUpload& upload = server.upload();
    int totalSize = server.arg("size").toInt();

    if (upload.status == UPLOAD_FILE_START) {
        fsUploadFile = SPIFFS.open(filename, "w");
        DBG_OUTPUT_PORT.printf(">> Receiving %s\n", filename);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (fsUploadFile) {
            fsUploadFile.write(upload.buf, upload.currentSize);
            DBG_OUTPUT_PORT.printf(">> %i/%i\n", upload.totalSize, totalSize);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (fsUploadFile) {
            fsUploadFile.close();
            writeMD5FileForFilename(filename);
            DBG_OUTPUT_PORT.printf(">> %i/%i\n", upload.totalSize, totalSize);
            DBG_OUTPUT_PORT.printf(">> Done.\n");
        }
    }
}
*/
void writeMD5FileForFilename(const char* filename) {
    File f = SPIFFS.open(filename, "r");
    if (f) {
        String md5sum = _md5sum(f);
        char newFilename[strlen(filename) + 4];
        sprintf(newFilename, "%s.md5", filename);
        _writeFile(newFilename, md5sum.c_str(), md5sum.length());
        f.close();
    }
}
/*
void handleDownloadFirmware() {
    WiFiClient client = server.client();
    
    client.print("HTTP/1.1 200 OK\r\n");
    client.print("Content-Type: text/plain\r\n");
    client.print("Content-Length: -1\r\n");
    client.print("Connection: close\r\n");
    client.print("Access-Control-Allow-Origin: *\r\n");
    client.print("\r\n");
    
    downloadFile(
        String(String(firmwareUrl) + ".md5").c_str(), 
        String(String(FIRMWARE_FILE) + ".orig.md5").c_str()
    );
    String msg = downloadFile(firmwareUrl, FIRMWARE_FILE);

    client.stop();
}
*/
/*
String downloadFile(const char* source, const char* target) {
    HTTPClient http;
    String returnValue;
    DBG_OUTPUT_PORT.printf(">> [HTTP] begin...\n");
    http.begin(source);
    DBG_OUTPUT_PORT.printf(">> [HTTP] GET...\n");
    int httpCode = http.GET();
    if (httpCode > 0) {
        DBG_OUTPUT_PORT.printf(">> [HTTP] GET... code: %d\n", httpCode);
        // file found at server
        if (httpCode == HTTP_CODE_OK) {
            // get lenght of document (is -1 when Server sends no Content-Length header)
            int len = http.getSize();
            
            File f = SPIFFS.open(target, "w");
            if (f) {
                // create buffer for read
                uint8_t buff[256] = { 0 };

                // get tcp stream
                WiFiClient * stream = http.getStreamPtr();

                // read all data from server
                while (http.connected() && (len > 0 || len == -1)) {
                    // get available data size
                    size_t size = stream->available();
                    size_t bytes2read;

                    if (size) {
                        bytes2read = ((size > sizeof(buff)) ? sizeof(buff) : size);
                        // read up to buff size bytes
                        int c = stream->readBytes(buff, bytes2read);
                        f.write(buff, bytes2read);

                        // write it to Serial
                        DBG_OUTPUT_PORT.printf("Got: %i/%i, %i\n", c, len, bytes2read);

                        if(len > 0) {
                            len -= c;
                        }
                    }
                    yield();
                }
                f.close();
                writeMD5FileForFilename(target);
            }

            DBG_OUTPUT_PORT.print("\n>> [HTTP] connection closed or file end.\n");
            if (len == 0) {
                returnValue = String("OK: file downloaded.");
            } else {
                returnValue = String("WARNING: file downloaded, but length unchecked.");
            }
        } else {
            returnValue = String("ERROR: file NOT downloaded.");
        }
    } else {
        DBG_OUTPUT_PORT.printf(">> [HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
        returnValue = String("ERROR: connect failed.");
    }

    http.end();
    return returnValue;
}
*/
void setupArduinoOTA() {
    DBG_OUTPUT_PORT.printf(">> Setting up ArduinoOTA...\n");
    
    ArduinoOTA.setPort(8266);
    ArduinoOTA.setHostname(host);

    if (strlen(otaPassword)) {
        ArduinoOTA.setPassword(otaPassword);
    }
    
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
/*
*/
/*
bool isAuthenticated() {
    if (!_isAuthenticated()) {
        server.requestAuthentication(BASIC_AUTH, "Secure Zone", "Please login!\n");
        return false;
    }
    return true;
}*/
/*
void handleAuthenticated(void (*handler)()) {
    if (isAuthenticated()) {
        handler();
    }
}
*/

void setupHTTPServer() {
    DBG_OUTPUT_PORT.printf(">> Setting up HTTP server...\n");

    server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        request->send(200);
    }, handleUpload);

    server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request){
        if(!_isAuthenticated(request)) {
            return request->requestAuthentication();
        }
        handleDownload(request);
    });

    AsyncStaticWebHandler* handler = &server
        .serveStatic("/", SPIFFS, "/")
        .setDefaultFile("index.html");
    // set authentication by configured user/pass later
    handler->setAuthentication("Test", "testtest");

/*
  */  
        /*
    server.on("/list", HTTP_GET, [](){
        handleAuthenticated(handleFileList);
    });
    server.on("/dump", HTTP_GET, [](){
        handleAuthenticated(handleDumpFirmware);
    });
    server.on("/flash", HTTP_GET, [](){
        handleAuthenticated(handleProgramFlash);
    });
    server.on("/download", HTTP_GET, [](){
        handleAuthenticated(handleDownloadFirmware);
    });
    server.on("/upload", HTTP_POST, []() {
        if (isAuthenticated()) {
            server.send(200, "text/plain", "");
        }
    }, handleUploadFirmware);
    server.on("/upload-index", HTTP_POST, []() {
        if (isAuthenticated()) {
            server.send(200, "text/plain", "");
        }
    }, handleUploadIndexHtml);
        
    server.onNotFound([]() {
        if (isAuthenticated()) {
            if (!handleFileRead(server.uri())) {
                server.send(404, "text/plain", "FileNotFound");
            }
        }
    });
    */
    server.begin();
}

void setupSPIFFS() {
    DBG_OUTPUT_PORT.printf(">> Setting up SPIFFS...\n");
    SPIFFS.begin();
    {
        Dir dir = SPIFFS.openDir("/");
        while (dir.next()) {    
            String fileName = dir.fileName();
            size_t fileSize = dir.fileSize();
            DBG_OUTPUT_PORT.printf(">> %s (%lu)\n", fileName.c_str(), fileSize);
        }
        DBG_OUTPUT_PORT.printf("\n");
    }
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
    
    if (DEBUG 
     || strlen(otaPassword)) 
    {
        setupArduinoOTA();
    }

    DBG_OUTPUT_PORT.println(">> Ready.");
}

void loop(void){
    ArduinoOTA.handle();
}
