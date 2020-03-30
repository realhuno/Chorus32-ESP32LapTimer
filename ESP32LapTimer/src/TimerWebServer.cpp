#include "TimerWebServer.h"

#include "settings_eeprom.h"
#include "ADC.h"
#include "RX5808.h"
#include "Calibration.h"
#include "Laptime.h"
#include "Comms.h"
#include "HardwareConfig.h"
#include "Wireless.h"
#include "Output.h"
#include "CrashDetection.h"

#include <esp_wifi.h>
#include <FS.h>
#include "SPIFFS.h"
#include <Update.h>
#include <ESPAsyncWebServer.h>

#define WEBSOCKET_BUF_SIZE 1500
static uint8_t websocket_buffer[WEBSOCKET_BUF_SIZE];
static int websocket_buffer_pos = 0;
SemaphoreHandle_t websocket_lock;

AsyncWebServer webServer(80);
AsyncWebSocket ws("/ws");

//flag to use from web update to reboot the ESP
//static bool shouldReboot = false;
static const char NOSPIFFS[] PROGMEM = "<h1>ERROR: Could not read the SPIFFS partition</h1>This means you either forgot to upload the data files or a previous update failed.<br>To fix this problem you have a few options:<h4>1 Arduino IDE</h4> Install the <b><a href=\"https://github.com/me-no-dev/arduino-esp32fs-plugin\">following plugin</a></b>.<br> Place the plugin file here: <b>\"&lt;path to your Arduino dir&gt;/tools/ESP32FS/tool/esp32fs.jar\"</b>.<br><br> Next select <b>Tools > ESP32 Sketch Data Upload</b>.<br>NOTE: This is a seperate upload to the normal arduino upload!<h4>2 Platformio</h4>Press the Upload Filesystem button or run \"pio run -e &lt;your board&gt; -t uploadfs\"<h4>3 Use this form</h4>Upload the spiffs file from the release site here:<br><br><form method='POST' action='/update' enctype='multipart/form-data'> <input type='file' name='update'> <input type='submit' value='Update'></form>";

static bool HasSPIFFsBegun = false;

static bool isHTTPUpdating = false;

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

void updateRx (int band, int channel, int rx) {
  setPilotBand(rx, band);
  setPilotChannel(rx, channel);
}


void send_laptimes(AsyncWebServerRequest* req) {
  // example json: '{"race_num": 5, "lap_data" : [ {"pilot" : 0, "laps" : [4, 2, 3]}]}'
  String json_string = "{\"race_mode\": " + String(isInRaceMode()) + ",\"race_num\" : " + String(getRaceNum()) + ", \"count_first\": " + String(getCountFirstLap());
  if(getActivePilots() > 0) {
    json_string += ", \"lap_data\" : [";
    for(int i = 0; i < MAX_NUM_PILOTS; ++i) {
      if(isPilotActive(i)) {
        json_string += String("{\"pilot\" : ") + i + ", \"laps\" : [";
        for(int j = 0; j < getCurrentLap(i); ++j) {
          json_string += getLaptimeRel(i, j + 1);
          if(j +1 != getCurrentLap(i)) {
            json_string += ",";
          }
        }
        json_string += "]},";
      }
    }
    json_string.remove(json_string.length() - 1); // remove last ,
    json_string += "]";
  }
  json_string += "}";
  req->send(200, "application/json", json_string);
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
  if(isHTTPUpdating) return; // ignore all incoming messages during update
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
  webServer.on("/recovery.html", HTTP_GET, [](AsyncWebServerRequest* req) {
      req->send(200, "text/html", NOSPIFFS);
    });

  if (!SPIFFS.exists("/index.html")) {
    Serial.println("SPIFFS filesystem was not found");
    webServer.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
      req->redirect("/recovery.html");
    });
  } else {
    webServer.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
      req->send(SPIFFS, "/index.html");
    });
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


  webServer.on("/get_laptimes", send_laptimes);
  webServer.on("/start_race", startRace_button);
  webServer.on("/stop_race", stopRace_button);

  webServer.on("/calibrateRSSI",calibrateRSSI);



  webServer.on("/update", HTTP_POST, [](AsyncWebServerRequest* req) {
    AsyncWebServerResponse *response = req->beginResponse((Update.hasError()) ? 400 : 200, "text/plain", (Update.hasError()) ? "FAIL" : "OK, module rebooting");
    response->addHeader("Connection", "close");
    req->send(response);
    Serial.println("off-updating");
    restart_esp();
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
      log_i("Update Start: %s on partition %d\n", filename.c_str(), partition);
      if (!Update.begin(UPDATE_SIZE_UNKNOWN, partition)) { //start with max available size
        log_e("%s\n", Update.errorString());
        isHTTPUpdating = false;
      }
    }
    if(!Update.hasError()){
      if(Update.write(data, len) != len){
        log_e("%s\n", Update.errorString());
        isHTTPUpdating = false;
      }
      uint8_t progress = (Update.progress() / Update.size()) * 100;
      sendUpdateProgress(progress);
    }
    if(final){
      if(Update.end(true)){
        Serial.printf("Update Success: %uB\n", index+len);
      } else {
        log_e("%s\n", Update.errorString());
      }
      isHTTPUpdating = false;
    }
  });

  webServer.begin();                           // Actually start the server
  Serial.println("HTTP server started");
  delay(1000);
}

bool isUpdating() {
  return isHTTPUpdating;
}
