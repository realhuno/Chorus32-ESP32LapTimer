#include "TimerWebServer.h"

#include "settings_eeprom.h"
#include "ADC.h"
#include "RX5808.h"
#include "Calibration.h"
#include "Laptime.h"
#include "Comms.h"
#include "HardwareConfig.h"
#include "Output.h"

#include <esp_wifi.h>
#include <DNSServer.h>
#include <FS.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include "SPIFFS.h"
#include <Update.h>
#include <ESPAsyncWebServer.h>

static const byte DNS_PORT = 53;
static IPAddress apIP(192, 168, 4, 1);
static DNSServer dnsServer;

#define WEBSOCKET_BUF_SIZE 1500
static uint8_t websocket_buffer[WEBSOCKET_BUF_SIZE];
static int websocket_buffer_pos = 0;
SemaphoreHandle_t websocket_lock;

AsyncWebServer webServer(80);
AsyncWebSocket ws("/ws");

//flag to use from web update to reboot the ESP
//static bool shouldReboot = false;
static const char NOSPIFFS[] PROGMEM = "You have not uploaded the SPIFFs filesystem!!!, Please install the <b><a href=\"https://github.com/me-no-dev/arduino-esp32fs-plugin\">following plugin</a></b>.<br> Place the plugin file here: <b>\"<path to your Arduino dir>/tools/ESP32FS/tool/esp32fs.jar\"</b>.<br><br> Next select <b>Tools > ESP32 Sketch Data Upload</b>.<br>NOTE: This is a seperate upload to the normal arduino upload!!!<br><br> The web interface will not work until you do this.";

static bool HasSPIFFsBegun = false;

static bool airplaneMode = false;
static bool isHTTPUpdating = false;

void airplaneModeOn();
void airplaneModeOff();

String getMacAddress() {
  byte mac[6];
  WiFi.macAddress(mac);
  String cMac = "";
  for (int i = 0; i < 6; ++i) {
    cMac += String(mac[i], HEX);
    if (i < 5)
      cMac += "-";
  }
  cMac.toUpperCase();
  Serial.print("Mac Addr:");
  Serial.println(cMac);
  return cMac;
}

String getContentType(String filename) { // convert the file extension to the MIME type
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".htm")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".gif")) return "image/gif";
  else if (filename.endsWith(".svg")) return "image/svg+xml";
  else if (filename.endsWith(".jpg")) return "image/jpeg";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".xml")) return "text/xml";
  else if (filename.endsWith(".pdf")) return "application/x-pdf";
  else if (filename.endsWith(".zip")) return "application/x-zip";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(AsyncWebServerRequest* req, String path) { // send the right file to the client (if it exists)
  // If a folder is requested, send the index file
  String contentType = getContentType(path);            // Get the MIME type
  if (SPIFFS.exists(path)) {
    req->send(SPIFFS, path, contentType);
    return true;
  }
  return false;                                         // If the file doesn't exist, return false
}

void InitWifiAP() {
  WiFi.begin();
  delay( 500 ); // If not used, somethimes following command fails
  WiFi.mode( WIFI_AP );
  uint8_t protocol = getWiFiProtocol() ? (WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N) : (WIFI_PROTOCOL_11B);
  ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_AP, protocol));
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  uint8_t channel = getWiFiChannel();
  if(channel < 1 || channel > 13) {
    channel = 1;
  }
  Serial.print("Starting wifi \"" WIFI_AP_NAME "\" on channel ");
  Serial.print(channel);
  Serial.print(" and mode ");
  Serial.println(protocol ? "bgn" : "b");
  WiFi.softAP(WIFI_AP_NAME, NULL, channel);
  // if DNSServer is started with "*" for domain name, it will reply with
  // provided IP to all DNS request
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);
}

void updateRx (int band, int channel, int rx) {
  setPilotBand(rx, band);
  setPilotChannel(rx, channel);
}

void calibrateRSSI(AsyncWebServerRequest* req) {
  rssiCalibration();
  req->redirect("/redirect.html");
}

void startRace_button(AsyncWebServerRequest* req) {
  Serial.println("Starting race...");
  startRace();
  req->send(200, "text/plain", "");
}

void stopRace_button(AsyncWebServerRequest* req) {
  Serial.println("Stopping race...");
  stopRace();
  req->send(200, "text/plain", "");
}

void onWebsocketEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  if(isHTTPUpdating) return; // ignore all incoming messages during update
  Serial.print("Got websocket message: ");
  Serial.write(data, len);
  Serial.println("");
  if(xSemaphoreTake(websocket_lock, portMAX_DELAY)){
    //Handle WebSocket event
    if(type == WS_EVT_DATA){
      //data packet
      AwsFrameInfo * info = (AwsFrameInfo*)arg;
      if(info->final && info->index == 0 && info->len == len){
        //the whole message is in a single frame and we got all of it's data
        // we'll ignore fragmented messages for now
        if(websocket_buffer_pos + len < WEBSOCKET_BUF_SIZE) {
          memcpy(websocket_buffer + websocket_buffer_pos, data, len);
          websocket_buffer_pos += len;
        }
      }
    }
    xSemaphoreGive(websocket_lock);
  }
}

void read_websocket(void* output) {
  if(xSemaphoreTake(websocket_lock, 1)){
    if(websocket_buffer_pos > 0) {
      output_t* out = (output_t*)output;
      out->handle_input_callback(websocket_buffer, websocket_buffer_pos);
      websocket_buffer_pos = 0;
    }
    xSemaphoreGive(websocket_lock);
  }
}

void send_websocket(void* output, uint8_t* data, size_t len) {
  ws.textAll(data, len);
}

void InitWebServer() {
  HasSPIFFsBegun = SPIFFS.begin();
  // attach AsyncWebSocket
  ws.onEvent(onWebsocketEvent);
  webServer.addHandler(&ws);
  websocket_lock = xSemaphoreCreateMutex();

  if (!SPIFFS.exists("/index.html")) {
    Serial.println("SPIFFS filesystem was not found");
    webServer.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
      req->send(200, "text/html", NOSPIFFS);
    });

    webServer.onNotFound([](AsyncWebServerRequest* req) {
      req->send(404, "text/plain", "404: Not Found");
    });

    webServer.begin();                           // Actually start the server
    Serial.println("HTTP server started");
    return;
  }


  webServer.onNotFound([](AsyncWebServerRequest* req) {

    if (
      (req->url() == "/generate_204") ||
      (req->url() == "/gen_204") ||
      (req->url() == "/library/test/success.html") ||
      (req->url() == "/hotspot-detect.html") ||
      (req->url() == "/connectivity-check.html")  ||
      (req->url() == "/check_network_status.txt")  ||
      (req->url() == "/ncsi.txt")
    ) {
      req->redirect("/");
    } else {
      // If the client requests any URI
      if (!handleFileRead(req, req->url()))                  // send it if it exists
        req->send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
    }
  });


  webServer.on("/start_race", startRace_button);
  webServer.on("/stop_race", stopRace_button);

  webServer.on("/calibrateRSSI",calibrateRSSI);


  webServer.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(SPIFFS, "/index.html");
  });

  webServer.on("/update", HTTP_POST, [](AsyncWebServerRequest* req) {
    AsyncWebServerResponse *response = req->beginResponse((Update.hasError()) ? 400 : 200, "text/plain", (Update.hasError()) ? "FAIL" : "OK, module rebooting");
    response->addHeader("Connection", "close");
    req->send(response);
    Serial.println("off-updating");
    ESP.restart();
  }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    isHTTPUpdating = true;
    if(!index) {
      int partition = data[0] == 0xE9 ? U_FLASH : U_SPIFFS;
      if(partition == U_SPIFFS) {
        // Since we don't have a magic number, we are checking the filename for "spiffs"
        if(strstr(filename.c_str(), "spiffs") == NULL) {
          partition = -1; // set partition to an invalid value
        }
      }
      Serial.printf("Update Start: %s on partition %d\n", filename.c_str(), partition);
      if (!Update.begin(UPDATE_SIZE_UNKNOWN, partition)) { //start with max available size
        Update.printError(Serial);
        isHTTPUpdating = false;
      }
    }
    if(!Update.hasError()){
      if(Update.write(data, len) != len){
        Update.printError(Serial);
        isHTTPUpdating = false;
      }
    }
    if(final){
      if(Update.end(true)){
        Serial.printf("Update Success: %uB\n", index+len);
      } else {
        Update.printError(Serial);
      }
      isHTTPUpdating = false;
    }
  });

  webServer.begin();                           // Actually start the server
  Serial.println("HTTP server started");
  delay(1000);
}

void updateWifi() {
  dnsServer.processNextRequest();
}

void airplaneModeOn() {
  // Enable Airplane Mode (WiFi Off)
  Serial.println("Airplane Mode On");
  WiFi.mode(WIFI_OFF);
  airplaneMode = true;
}

void airplaneModeOff() {
  // Disable Airplane Mode (WiFi On)
  Serial.println("Airplane Mode OFF");
  InitWifiAP();
  InitWebServer();
  airplaneMode = false;
}

// Toggle Airplane mode on and off based on current state
void toggleAirplaneMode() {
  if (!airplaneMode) {
    airplaneModeOn();
  } else {
    airplaneModeOff();
  }
}

bool isAirplaneModeOn() {
  return airplaneMode;
}

bool isUpdating() {
  return isHTTPUpdating;
}
