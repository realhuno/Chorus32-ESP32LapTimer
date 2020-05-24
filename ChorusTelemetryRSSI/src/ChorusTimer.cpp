#include <WiFi.h>
#include <WiFiMulti.h>
#include <esp_wifi.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>

//OTA
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>


#define WIFI_AP_NAME "Chorus32 LapTimer"

#define UART_TX 15
#define UART_RX 4

#define PROXY_PREFIX 'P' // used to configure any proxy (i.e. this device ;))
#define PROXY_CONNECTION_STATUS 'c'
#define PROXY_WIFI_STATUS 'w'
#define PROXY_WIFI_RSSI 't'


WiFiClient client;
WiFiMulti wifiMulti;

#define MAX_BUF 1500
char buf[MAX_BUF];
uint32_t buf_pos = 0;
int real_rssi=0;
void chorus_connect() {
	if(WiFi.isConnected()) {
        

		if(WiFi.SSID()=="Chorus32 LapTimer"){
           if(client.connect("192.168.4.1", 9000)) {
			Serial.println("Connected to chorus via tcp 192.168.4.1");
		}
		}


		if(WiFi.SSID()=="Laptimer"){
           if(client.connect("192.168.0.141", 9000)) {
			Serial.println("Connected to chorus via tcp 192.168.0.141");
		}
		}

		if(WiFi.SSID()=="A1-7FB051"){
           if(client.connect("10.0.0.50", 9000)) {
			Serial.println("Connected to chorus via tcp 10.0.0.50");
		}
		}

	}
}

void setup() {
	Serial.begin(115200);
	Serial1.begin(115200, SERIAL_8N1, UART_RX, UART_TX, true);
	//WiFi.begin(WIFI_AP_NAME);
	
	wifiMulti.addAP("Chorus32 LapTimer", "");
    wifiMulti.addAP("Laptimer", "laptimer");
    wifiMulti.addAP("A1-7FB051", "hainz2015");

    Serial.println("Connecting Wifi...");
    if(wifiMulti.run() == WL_CONNECTED) {
        Serial.println("");
        Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
    }
	






	delay(500);

	memset(buf, 0, 1500 * sizeof(char));
	//OTA
	  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();
}

void loop() {
	ArduinoOTA.handle();
	real_rssi=WiFi.RSSI();
    if(wifiMulti.run() != WL_CONNECTED) {
        Serial.println("WiFi not connected!");
        delay(1000);
    }


	while(client.connected() && client.available()) {
		// Doesn't include the \n itself
		String line = client.readStringUntil('\n');

		// Only forward specific messages for now
		//if(line[2] == 'L') {
			Serial1.println(line);
			Serial.printf("Forwarding to taranis: %s\n", line.c_str());
		//}
	}

	while(Serial1.available()) {
		if(buf_pos >= MAX_BUF -1 ) {
			buf_pos = 0; // clear buffer when full
			break;
		}
		buf[buf_pos++] = Serial1.read();
		if(buf[buf_pos - 1] == '\n') {
			// catch proxy command
			if(buf[0] == 'P') {
				switch(buf[3]) {
					case PROXY_WIFI_STATUS:
						Serial1.printf("%cS*%c%1x\n", PROXY_PREFIX, PROXY_WIFI_STATUS, WiFi.status());
						break;
                    case PROXY_WIFI_RSSI:
						Serial1.printf("%cS*%c%2x\n", PROXY_PREFIX, PROXY_WIFI_RSSI, abs(real_rssi*-1));
						Serial.println(real_rssi);
						break;
					case PROXY_CONNECTION_STATUS:
						Serial1.printf("%cS*%c%1x\n", PROXY_PREFIX, PROXY_CONNECTION_STATUS, client.connected());
						break;
				}
			}
			else if((buf[0] == 'E' && (buf[1] == 'S' || buf[1] == 'R')) || buf[0] == 'S' || buf[0] == 'R') {
				Serial.print("Forwarding to chorus: ");
				Serial.write((uint8_t*)buf, buf_pos);
				client.write(buf, buf_pos);
			}
			buf_pos = 0;
		}
	}
    
	if(!client.connected()) {
		delay(1000);
		Serial.println("Lost tcp connection! Trying to reconnect");
		Serial.println(WiFi.localIP());
		Serial.println(WiFi.SSID());
		chorus_connect();
	}










}