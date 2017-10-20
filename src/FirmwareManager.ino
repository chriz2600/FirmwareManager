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
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <SPIFlash.h>

#define CS 16
#define DBG_OUTPUT_PORT Serial
#define PAGES 8192 // 8192 pages x 256 bytes = 2MB = 16MBit

char ssid[64];
char password[64];
char otaPassword[64]; 
const char* host = "dc-firmware-manager";
const char* WiFiAPPSK = "geheim1234";
IPAddress ipAddress( 192, 168, 4, 1 );

const uint8_t empty_buffer[256] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

ESP8266WebServer server(80);
SPIFlash flash(CS);
//holds the current upload
File fsUploadFile;

void print_page_bytes(unsigned int page, const byte *page_buffer) {
    char buf[10];
    DBG_OUTPUT_PORT.print(String(page));
    DBG_OUTPUT_PORT.println(":");
    for (int i = 0; i < 16; ++i) {
        for (int j = 0; j < 16; ++j) {
            sprintf(buf, "%02x", page_buffer[i * 16 + j]);
            DBG_OUTPUT_PORT.print(buf);
        }
        DBG_OUTPUT_PORT.println();
    }
}

//format bytes
String formatBytes(size_t bytes){
    if (bytes < 1024){
        return String(bytes)+"B";
    } else if(bytes < (1024 * 1024)){
        return String(bytes/1024.0)+"KB";
    } else if(bytes < (1024 * 1024 * 1024)){
        return String(bytes/1024.0/1024.0)+"MB";
    } else {
        return String(bytes/1024.0/1024.0/1024.0)+"GB";
    }
}

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
}

bool handleFileRead(String path){
    if (path.endsWith("/")) {
        path += "index.htm";
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

void handleFileUpload(){
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        fsUploadFile = SPIFFS.open("flash.bin", "w");
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (fsUploadFile) {
            fsUploadFile.write(upload.buf, upload.currentSize);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (fsUploadFile) {
            fsUploadFile.close();
        }
    }
}

void _readCredentials(const char *filename, char *target) {
    bool exists = SPIFFS.exists(filename);
    if (exists) {
        File f = SPIFFS.open(filename, "r");
        if (f) {
            f.readString().toCharArray(target, 64);
            f.close();
        }
    }
}

void readCredentials(void) {
    _readCredentials("/ssid.txt", ssid);
    _readCredentials("/password.txt", password);
    _readCredentials("/ota-pass.txt", otaPassword);
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
    DBG_OUTPUT_PORT.print("SSID: ");
    DBG_OUTPUT_PORT.println(AP_NameChar);
    DBG_OUTPUT_PORT.print("APPSK: ");
    DBG_OUTPUT_PORT.println(WiFiAPPSK);
}

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

void initBuffer(uint8_t *buffer) {
    for (int i = 0 ; i < 256 ; i++) { 
        buffer[i] = 0xff; 
        yield(); 
    }
}

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
    
    File f = SPIFFS.open("/output_file.rbf", "r");
    if (f) {
        flash.enable();
        flash.chip_erase();
        while (f.readBytes((char *) buffer, 256) && page < PAGES) {
            server.sendContent(String(page) + "\n");
            flash.page_write(page, buffer);
            // cleanup buffer
            initBuffer(buffer);
            page++;
        }
        flash.disable();
        f.close();
    }
    
    server.sendContent(""); // *** END 1/2 ***
    server.client().stop(); // *** END 2/2 ***
}

void handleDownloadFirmware() {
    uint8_t page_buffer[256];
    unsigned int last_page = 0;
    const char* temp_filename = "/flash.raw.bin";
    
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    server.setContentLength(CONTENT_LENGTH_UNKNOWN); // *** BEGIN ***
    server.send(200, "text/plain", "");
    
    File f = SPIFFS.open(temp_filename, "w");
    if (f) {
        server.sendContent("read from flash:\n");
        flash.enable();
        for (unsigned int i = 0; i < PAGES; ++i) {
            server.sendContent(String(i));
            server.sendContent("\n");
            flash.page_read(i, page_buffer);
            f.write(page_buffer, 256); 
            if (strcmp((const char*) page_buffer, (const char*) empty_buffer) != 0) {
                last_page = i;
            }
        }
        flash.disable();
        server.sendContent("last_page:" + String(last_page) + "\n");
        //copyBlockwise(temp_filename, "/flash.bin", last_page);
        //SPIFFS.remove(temp_filename);
        f.close();
    }
    
    server.sendContent(""); // *** END 1/2 ***
    server.client().stop(); // *** END 2/2 ***
}

void setupArduinoOTA() {

    ArduinoOTA.setPort(8266);
    ArduinoOTA.setHostname(host);
    ArduinoOTA.setPassword(otaPassword);
    
    ArduinoOTA.onStart([]() {
        Serial.println("Start");
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    ArduinoOTA.begin();
    Serial.println("Ready");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

void setup(void){
    DBG_OUTPUT_PORT.begin(115200);
    DBG_OUTPUT_PORT.print("\n");
    DBG_OUTPUT_PORT.setDebugOutput(true);
    SPIFFS.begin();
    {
        Dir dir = SPIFFS.openDir("/");
        while (dir.next()) {    
            String fileName = dir.fileName();
            size_t fileSize = dir.fileSize();
            DBG_OUTPUT_PORT.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
        }
        DBG_OUTPUT_PORT.printf("\n");
    }
    
    readCredentials();
    
    //WIFI INIT
    DBG_OUTPUT_PORT.printf("Connecting to %s\n", ssid);
    if (String(WiFi.SSID()) != String(ssid)) {
        WiFi.begin(ssid, password);
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
    if (!success) {
        // setup AP mode to configure ssid and password
        setupAPMode();
    } else {
        ipAddress = WiFi.localIP();
        DBG_OUTPUT_PORT.println("");
        DBG_OUTPUT_PORT.print("Connected! IP address: ");
        DBG_OUTPUT_PORT.println(ipAddress);
    }
    
    if (MDNS.begin(host, ipAddress)) {
        DBG_OUTPUT_PORT.println("mDNS started");
        MDNS.addService("http", "tcp", 80);
        DBG_OUTPUT_PORT.print("Open http://");
        DBG_OUTPUT_PORT.print(host);
        DBG_OUTPUT_PORT.println(".local/edit to see the file browser");
    }
    
    server.on("/flash-firmware", HTTP_GET, handleProgramFlash);
    server.on("/download-firmware", HTTP_GET, handleDownloadFirmware);
    server.onNotFound([](){
        if (!handleFileRead(server.uri())) {
            server.send(404, "text/plain", "FileNotFound");
        }
    });

    if (strlen(otaPassword)) {
        setupArduinoOTA();
    }
}

void loop(void){
    server.handleClient();
    ArduinoOTA.handle();
}
