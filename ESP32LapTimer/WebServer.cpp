#include "WebServer.h"

#include "settings_eeprom.h"
#include "ADC.h"
#include "RX5808.h"
#include "Calibration.h"
#include "Laptime.h"
#include "Comms.h"
#include "HardwareConfig.h"

#include <esp_wifi.h>
#include <DNSServer.h>
#include <FS.h>
#include <WiFiUdp.h>
#include "WebServer.h"
#include <WebServer.h>
#include <ESPmDNS.h>
#include "SPIFFS.h"
#include <Update.h>

static const byte DNS_PORT = 53;
static IPAddress apIP(192, 168, 4, 1);
static DNSServer dnsServer;
static WebServer webServer(80);
static WiFiClient client = webServer.client();

//flag to use from web update to reboot the ESP
//static bool shouldReboot = false;
static const char NOSPIFFS[] PROGMEM = "You have not uploaded the SPIFFs filesystem!!!, Please install the <b><a href=\"https://github.com/me-no-dev/arduino-esp32fs-plugin\">following plugin</a></b>.<br> Place the plugin file here: <b>\"<path to your Arduino dir>/tools/ESP32FS/tool/esp32fs.jar\"</b>.<br><br> Next select <b>Tools > ESP32 Sketch Data Upload</b>.<br>NOTE: This is a seperate upload to the normal arduino upload!!!<br><br> The web interface will not work until you do this.";


static bool firstRedirect = true;
static bool HasSPIFFsBegun = false;

static bool HTTPupdating = false;
static bool airplaneMode = false;

//////////////////////////////////////////////////////////////////

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

bool handleFileRead(String path) { // send the right file to the client (if it exists)
  HTTPupdating = true;
  // If a folder is requested, send the index file
  String contentType = getContentType(path);            // Get the MIME type
  if (SPIFFS.exists(path)) {
    File file = SPIFFS.open(path, "r");                 // Open it
    size_t sent = webServer.streamFile(file, contentType); // And send it to the client
    (void)sent;
    file.close();
    HTTPupdating = false;
    return true;
  }
  HTTPupdating = false;
  return false;                                         // If the file doesn't exist, return false
}

void InitWifiAP() {
  HTTPupdating = true;
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
  HTTPupdating = false;
}

void updateRx (int band, int channel, int rx) {
  setRXBandPilot(rx, band);
  EepromSettings.RXBand[rx] = getRXBandPilot(rx);
  setRXChannelPilot(rx, channel);
  EepromSettings.RXChannel[rx] = getRXChannelPilot(rx);
}

void SendStatusVars() {
  webServer.send(200, "application/json", "{\"Var_VBAT\": " + String(getVbatFloat(), 2) + ", \"Var_WifiClients\": 1, \"Var_CurrMode\": \"IDLE\"}");
}

void SendStaticVars() {
  // TODO: implement dynamic lap number
  String sendSTR = "{\"num_laps\": " + String(5) + ", \"NumRXs\": " + String(getNumReceivers() - 1) + ", \"ADCVBATmode\": " + String(getADCVBATmode()) + ", \"RXFilterCutoff\": " + String(getRXADCfilterCutoff()) + ", \"ADCcalibValue\": " + String(getVBATcalibration(), 3) + ", \"RSSIthreshold\": " + String(getRSSIThreshold(0)) + ", \"WiFiChannel\": " + String(getWiFiChannel()) + ", \"WiFiProtocol\": " + String(getWiFiProtocol());
  sendSTR = sendSTR + ",\"Band\":{";
  for (int i = 0; i < getNumReceivers(); i++) {
    sendSTR = sendSTR + "\"" + i + "\":" + EepromSettings.RXBand[i];
    if (getNumReceivers() > 1 && getNumReceivers() - i > 1) {
      sendSTR = sendSTR + ",";
    }
  }
  sendSTR = sendSTR + "},";
  sendSTR = sendSTR + "\"Channel\":{";
  for (int i = 0; i < getNumReceivers(); i++) {
    sendSTR = sendSTR + "\"" + i + "\":" + EepromSettings.RXChannel[i];
    if (getNumReceivers() > 1 && getNumReceivers() - i > 1) {
      sendSTR = sendSTR + ",";
    }
  }
  sendSTR = sendSTR + "}";
  sendSTR += ",\"pilot_data\" : [";
  for(int i = 0; i < MAX_NUM_PILOTS; ++i) {
    sendSTR += "{\"number\" : " + String(i);
    sendSTR += ", \"enabled\" : " + String(isPilotActive(i));
    sendSTR += ", \"multiplex_off\" : " + String(isPilotMultiplexOff(i));
    sendSTR += ", \"band\" : " + String(getRXBandPilot(i));
    sendSTR += ", \"channel\" : " + String(getRXChannelPilot(i));
    sendSTR += ", \"threshold\" : " + String(getRSSIThreshold(i) / 12.0);
    sendSTR += "}";
    if(i + 1 < MAX_NUM_PILOTS) {
      sendSTR += ",";
    }
  }

  sendSTR = sendSTR +  "]}";
  webServer.send(200, "application/json", sendSTR);
}

void send_laptimes() {
  // example json: '{"race_num": 5, "lap_data" : [ {"pilot" : 0, "laps" : [4, 2, 3]}]}'
  String json_string = "{\"race_num\" : " + String(getRaceNum()) + ", \"count_first\": " + String(getCountFirstLap());
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
  json_string += "]}";
  webServer.send(200, "application/json", json_string);
}

void ProcessGeneralSettingsUpdate() {
  String NumRXs = webServer.arg("NumRXs");
  EepromSettings.NumReceivers = (byte)NumRXs.toInt();

  for(int i = 0; i < MAX_NUM_PILOTS; ++i) {
    String enabled = webServer.arg("pilot_enabled_" + String(i));
    String multiplex_off = webServer.arg("pilot_multuplex_off_" + String(i));
    setPilotActive(i, enabled == "on");
    setilotMultiplexOff(i, multiplex_off == "on");
    
    String Band_str = webServer.arg("band" + String(i));
    String Channel_str = webServer.arg("channel" + String(i));
    int band = (uint8_t)Band_str.toInt();
    int channel = (uint8_t)Channel_str.toInt();
    updateRx(band, channel, i);
    
    String Rssi = webServer.arg("RSSIthreshold" + String(i));
    int rssi = (byte)Rssi.toInt();
    int value = rssi * 12;
    EepromSettings.RSSIthresholds[i] = value;
    setRSSIThreshold(i, value);
  }

  webServer.sendHeader("Connection", "close");
  File file = SPIFFS.open("/redirect.html", "r");                 // Open it
  size_t sent = webServer.streamFile(file, "text/html"); // And send it to the client
  (void)sent;
  file.close();
  setSaveRequired();
}

void calibrateRSSI() {
  rssiCalibration();
}
void eepromReset(){
  EepromSettings.defaults();
  webServer.sendHeader("Connection", "close");
  File file = SPIFFS.open("/redirect.html", "r");
  webServer.streamFile(file, "text/html");
}

void ProcessVBATModeUpdate() {
  String inADCVBATmode = webServer.arg("ADCVBATmode");
  String inADCcalibValue = webServer.arg("ADCcalibValue");

  setADCVBATmode((ADCVBATmode_)(byte)inADCVBATmode.toInt());
  setVBATcalibration(inADCcalibValue.toFloat());

  EepromSettings.ADCVBATmode = getADCVBATmode();
  EepromSettings.VBATcalibration = getVBATcalibration();
  setSaveRequired();

  webServer.sendHeader("Connection", "close");
  File file = SPIFFS.open("/redirect.html", "r");                 // Open it
  size_t sent = webServer.streamFile(file, "text/html"); // And send it to the client
  (void)sent;
  file.close();
  setSaveRequired();
}

void ProcessADCRXFilterUpdate() {
  String inRXFilter = webServer.arg("RXFilterCutoff");
  setRXADCfilterCutoff(inRXFilter.toInt());
  EepromSettings.RXADCfilterCutoff = getRXADCfilterCutoff();

  webServer.sendHeader("Connection", "close");
  File file = SPIFFS.open("/redirect.html", "r");                 // Open it
  size_t sent = webServer.streamFile(file, "text/html"); // And send it to the client
  (void)sent;
  file.close();
  setSaveRequired();
  setPilotFilters(getRXADCfilterCutoff());
}

void ProcessWifiSettings() {
  String inWiFiChannel = webServer.arg("WiFiChannel");
  String inWiFiProtocol = webServer.arg("WiFiProtocol");

  EepromSettings.WiFiProtocol = inWiFiProtocol.toInt();
  EepromSettings.WiFiChannel = inWiFiChannel.toInt();

  webServer.sendHeader("Connection", "close");
  File file = SPIFFS.open("/redirect.html", "r");                 // Open it
  size_t sent = webServer.streamFile(file, "text/html"); // And send it to the client
  (void)sent;
  file.close();
  setSaveRequired();
  airplaneModeOn();
  airplaneModeOff();
}

void startRace_button() {
	Serial.println("Starting race...");
	startRace();
	webServer.send(200, "text/plain", "");
}

void stopRace_button() {
	Serial.println("Stopping race...");
	stopRace();
	webServer.send(200, "text/plain", "");
}


void InitWebServer() {


  HasSPIFFsBegun = SPIFFS.begin();
  //delay(1000);

  if (!SPIFFS.exists("/index.html")) {
    Serial.println("SPIFFS filesystem was not found");
    webServer.on("/", HTTP_GET, []() {
      webServer.send(200, "text/html", NOSPIFFS);
    });

    webServer.onNotFound([]() {
      webServer.send(404, "text/plain", "404: Not Found");
    });

    webServer.begin();                           // Actually start the server
    Serial.println("HTTP server started");
    //client.setNoDelay(true);
    return;
  }


  webServer.onNotFound([]() {

    if (
      (webServer.uri() == "/generate_204") ||
      (webServer.uri() == "/gen_204") ||
      (webServer.uri() == "/library/test/success.html") ||
      (webServer.uri() == "/hotspot-detect.html") ||
      (webServer.uri() == "/connectivity-check.html")  ||
      (webServer.uri() == "/check_network_status.txt")  ||
      (webServer.uri() == "/ncsi.txt")
    ) {
      webServer.sendHeader("Location", "/", true);  //Redirect to our html web page
      if (firstRedirect) {
        webServer.sendHeader("Location", "/", true);  //Redirect to our html web page
        webServer.send(301, "text/plain", "");
      } else {
        webServer.send(404, "text/plain", "");
      }
    } else {

      // If the client requests any URI
      if (!handleFileRead(webServer.uri()))                  // send it if it exists
        webServer.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
    }
  });

  webServer.on("/StatusVars", SendStatusVars);
  webServer.on("/StaticVars", SendStaticVars);
  
  webServer.on("/get_laptimes", send_laptimes);
  webServer.on("/start_race", startRace_button);
  webServer.on("/stop_race", stopRace_button);

  webServer.on("/updateGeneral", ProcessGeneralSettingsUpdate);
  webServer.on("/updateFilters", ProcessADCRXFilterUpdate);
  webServer.on("/ADCVBATsettings", ProcessVBATModeUpdate);
  webServer.on("/calibrateRSSI",calibrateRSSI);
  webServer.on("/eepromReset",eepromReset);
  
  webServer.on("/WiFisettings", ProcessWifiSettings);

  webServer.on("/", HTTP_GET, []() {
    firstRedirect = false; //wait for it to hit the index page one time
    HTTPupdating = true;
    webServer.sendHeader("Connection", "close");
    File file = SPIFFS.open("/index.html", "r");
    // Open it
    size_t sent = webServer.streamFile(file, "text/html"); // And send it to the client
    (void)sent;
    file.close();
    HTTPupdating = false;
  });


  webServer.on("/update", HTTP_POST, []() {
    HTTPupdating = true;
    Serial.println("off-updating");
    webServer.sendHeader("Connection", "close");
    webServer.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK, module rebooting");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = webServer.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.setDebugOutput(true);
      Serial.printf("Update: %s\n", upload.filename.c_str());
      uint32_t maxSketchSpace = 0x140000;

      if (!Update.begin(maxSketchSpace)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
      Serial.setDebugOutput(false);
    }
    yield();
  });


  webServer.begin();                           // Actually start the server
  Serial.println("HTTP server started");
  client.setNoDelay(1);
  delay(1000);
}

void updateWifi() {
  dnsServer.processNextRequest();
  webServer.handleClient();
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
