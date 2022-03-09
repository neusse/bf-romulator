#include "server.h"
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <Arduino.h>
#include <EEPROM.h>
#include <LittleFS.h>
#include "libRomulatorProgrammer.h"
#include "libRomulatorDebug.h"

const char *ssid = "romulator";
const char *password = "password";
const char* hostname = "romulator.local";

bool AP;
File fsUploadFile;
ESP8266WebServer server(80);
extern RomulatorProgrammer _programmer;
bool _connected;
int _programBufferSize = 0;
uint8_t _programBuffer[256];

extern void programFirmware();

struct settings {
    char ssid[30];
    char password[30];
} user_wifi = {};

void outputString(char* string) {
    
}

void handlePortal() {
    if (AP) 
    {
        if (server.method() == HTTP_POST) {
            strncpy(user_wifi.ssid,     server.arg("ssid").c_str(),     sizeof(user_wifi.ssid) );
            strncpy(user_wifi.password, server.arg("password").c_str(), sizeof(user_wifi.password) );
            user_wifi.ssid[server.arg("ssid").length()] = user_wifi.password[server.arg("password").length()] = '\0';
            EEPROM.put(0, user_wifi);
            EEPROM.commit();
            server.send(200,   "text/html",  "<!doctype html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>Wifi Setup</title><style>*,::after,::before{box-sizing:border-box;}body{margin:0;font-family:'Segoe UI',Roboto,'Helvetica Neue',Arial,'Noto Sans','Liberation Sans';font-size:1rem;font-weight:400;line-height:1.5;color:#212529;background-color:#f5f5f5;}.form-control{display:block;width:100%;height:calc(1.5em + .75rem + 2px);border:1px solid #ced4da;}button{border:1px solid transparent;color:#fff;background-color:#007bff;border-color:#007bff;padding:.5rem 1rem;font-size:1.25rem;line-height:1.5;border-radius:.3rem;width:100%}.form-signin{width:100%;max-width:400px;padding:15px;margin:auto;}h1,p{text-align: center}</style> </head> <body><main class='form-signin'> <h1>Wifi Setup</h1> <br/> <p>Your settings have been saved successfully!<br />Please restart the device.</p></main></body></html>" );
        } else {
            File fp = LittleFS.open("/wifi.html", "r");
            String s = fp.readString();
            //server.send(200,   "text/html", "<!doctype html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>Wifi Setup</title> <style>*,::after,::before{box-sizing:border-box;}body{margin:0;font-family:'Segoe UI',Roboto,'Helvetica Neue',Arial,'Noto Sans','Liberation Sans';font-size:1rem;font-weight:400;line-height:1.5;color:#212529;background-color:#f5f5f5;}.form-control{display:block;width:100%;height:calc(1.5em + .75rem + 2px);border:1px solid #ced4da;}button{cursor: pointer;border:1px solid transparent;color:#fff;background-color:#007bff;border-color:#007bff;padding:.5rem 1rem;font-size:1.25rem;line-height:1.5;border-radius:.3rem;width:100%}.form-signin{width:100%;max-width:400px;padding:15px;margin:auto;}h1{text-align: center}</style> </head> <body><main class='form-signin'> <form action='/' method='post'> <h1 class=''>Wifi Setup</h1><br/><div class='form-floating'><label>SSID</label><input type='text' class='form-control' name='ssid'> </div><div class='form-floating'><br/><label>Password</label><input type='password' class='form-control' name='password'></div><br/><br/><button type='submit'>Save</button><p style='text-align: right'><a href='https://www.mrdiy.ca' style='color: #32C5FF'>mrdiy.ca</a></p></form></main> </body></html>" );
            server.send(200, "text/html", s);
        }
    }
    else
    {
        // handle incoming file upload
        if (server.method() == HTTP_POST) {
            server.send(200, "text/html", "is a post");
        } else {
            File fp = LittleFS.open("/romulator.html", "r");
            String s = fp.readString();
            server.send(200, "text/html", s);
        }

    }
}

void handleFileUpload()
{
    HTTPUpload& upload = server.upload();
    if(upload.status == UPLOAD_FILE_START)
    {
        String filename = upload.filename;
        Serial.printf("upload filename: %s\n", filename.c_str());
        if (!filename.startsWith("/")) 
        {
            filename = "/"+filename;
        }
        Serial.print("handleFileUpload Name: "); Serial.println(filename);
        fsUploadFile = LittleFS.open(filename, "w");            // Open the file for writing in SPIFFS (create if it doesn't exist)
        filename = String();
    } 
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if(fsUploadFile)
        {
            fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
        }
    } 
    else if(upload.status == UPLOAD_FILE_END) 
    {
        if(fsUploadFile) 
        {                                    // If the file was successfully created
            fsUploadFile.close();                               // Close the file again
            Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
            // start programming from uploaded file
            _programmer.beginProgrammingFromFile((char*)upload.filename.c_str());
            server.send(200, "text/html", "done uploading!");
        } 
        else 
        {
            server.send(500, "text/plain", "500: couldn't create file");
        }
    }
}

void handleWriteMemory()
{
    Serial.printf("handleWriteMemory\n");
    HTTPUpload& upload = server.upload();
    if(upload.status == UPLOAD_FILE_START)
    {
        Serial.printf("start memory upload.\n");
        fsUploadFile = LittleFS.open("/memory.bin", "w");
    } 
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if(fsUploadFile)
        {
            fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
        }
    } 
    else if(upload.status == UPLOAD_FILE_END) 
    {
        if(fsUploadFile) 
        {                                    // If the file was successfully created
            fsUploadFile.close();                               // Close the file again
            Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
            // start programming from uploaded file
            romulatorWriteMemoryFromFile();
            server.send(200, "text/html", "done writing!");
        } 
        else 
        {
            server.send(500, "text/plain", "500: couldn't create file");
        }
    }
}

void handleRoot() {
    server.send(200, "text/html", "<h1>You are connected</h1>");
}

void handleProgress() {
    char progressStr[32];
    int progressPct = _programmer.getProgrammingPercentage();
    sprintf(progressStr, "%d", progressPct);
    server.send(200, "text/html", progressStr);
}

void handleHalt() {
    Serial.printf("halt\n");
    romulatorInitDebug();
    romulatorHaltCpu();
    server.send(200, "text/html", "halted");
}

void handleRun() {
    Serial.printf("run\n");
    romulatorStartCpu();
    romulatorSetInput();
    server.send(200, "text/html", "cpu running");
}

void handleReadMemory() {
    romulatorInitDebug();
    romulatorHaltCpu();
    romulatorReadMemoryToFile();
    romulatorStartCpu();

    File fp = LittleFS.open("/memory.bin", "r");
    server.sendHeader("content-disposition", "attachment; filename=\"memory.bin\"");
    server.send(200, "application/octet-stream", &fp, fp.size());
    fp.close();
}

void handleReset() {
    romulatorResetDevice();
    server.send(200, "text/html", "device reset.");
}

void startServer()
{
    EEPROM.begin(sizeof(struct settings));
    EEPROM.get(0, user_wifi);

    WiFi.mode(WIFI_STA);
    WiFi.hostname("romulator");

    delay(1000);
    Serial.printf("connecting %s %s\n", user_wifi.ssid, user_wifi.password);
    WiFi.begin(user_wifi.ssid, user_wifi.password);
    _connected = false;
    _programBufferSize = 0;

    //byte tries = 0;
    AP = false;

    /*
    while (WiFi.status() != WL_CONNECTED) 
    {
        Serial.printf(".");
        delay(1000);
        if (tries++ > 20) 
        {
            Serial.printf("Starting access point.\n");
            AP = true;
            WiFi.mode(WIFI_AP);
            WiFi.softAP(ssid, "");
            break;
        }
    }

    if (AP) {
        Serial.printf("AP mode, ip address is %s\n", WiFi.softAPIP().toString().c_str());
    } else {
        Serial.printf("connected, ip address %s\n", WiFi.localIP().toString().c_str());
    }

    server.on("/",  handlePortal);
    server.on("/program", handleProgram);
    server.on("/upload", HTTP_POST, [](){server.send(200);}, handleFileUpload);
    server.begin();
    Serial.println("HTTP server started.\n");
    */
}

void handleClient()
{
    if (!_connected)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.printf("connected, ip address %s\n", WiFi.localIP().toString().c_str());
            server.on("/",  handlePortal);
            server.on("/upload", HTTP_POST, [](){server.send(200);}, handleFileUpload);
            server.on("/progress", handleProgress);
            server.on("/halt", handleHalt);
            server.on("/run", handleRun);
            server.on("/readmemory", handleReadMemory);
            server.on("/reset", handleReset);
            server.on("/writememory", HTTP_POST, [](){server.send(200);}, handleWriteMemory);
            server.begin();
            Serial.println("HTTP server started.\n");
            _connected = true;
        }
        else
        {
            Serial.printf(".");
            return;
        }
    }
    server.handleClient();
}