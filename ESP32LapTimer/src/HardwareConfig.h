#pragma once

#include <stdint.h>

/// These are all the available targets
#define BOARD_DEFAULT 1
#define BOARD_OLD 2
#define BOARD_TTGO_LORA 3

///Define the board used here
///For jye's PCB v2.x the value doesn't need to be changed
///If you are using v1 of jye's PCB or used the wiring diagram you'll need to change this to "BOARD_OLD"
///To define your own custom board take a look at the "targets" directory
#ifndef BOARD
#define BOARD BOARD_DEFAULT
#endif

/// If your setup doesn't use an OLED remove or comment the following line
#define OLED

// Selects the wifi mode to operate in.
// One of these must be uncommented.
//
#define WIFI_MODE_ACCESSPOINT
// For now the AP name needs to be defined regardless of mode.
#define WIFI_AP_NAME "Chorus32 LapTimer"

// When in client mode you also need to specify the
// ssid and password.
//#define WIFI_MODE_CLIENT
// For now the ssid and password needs to be defined regardless of mode
#define WIFI_SSID "testnetwork"
#define WIFI_PASSWORD "testpassword"

/// Enables Bluetooth support. Disabled by default. If you enable it you might need to change the partition scheme to "Huge APP"
//#define USE_BLUETOOTH
// For now the bluetooth name needs to be defined regardless of if it's enabled or not
#define BLUETOOTH_NAME WIFI_AP_NAME
/// Outputs all messages on the serial port. Used to use Livetime via USB
#define USE_SERIAL_OUTPUT

// Enable TCP support. Currently this needs a special version of the app: https://github.com/Smeat/Chorus-RF-Laptimer/releases/tag/tcp_support
//#define USE_TCP

// Enables the ArduinoOTA service. It allows flashing over WiFi and enters an emergency mode if a crashloop is detected.
//#define USE_ARDUINO_OTA

// BELOW ARE THE ADVANCED SETTINGS! ONLY CHANGE THEM IF YOU KNOW WHAT YOUR ARE DOING!

#define EEPROM_VERSION_NUMBER 9 // Increment when eeprom struct modified
#define EEPROM_COMMIT_DELAY_MS 10000 // the time between checks to save the eeprom in ms
#define MAX_NUM_RECEIVERS 6
#define VOLTAGE_UPDATE_INTERVAL_MS 1000 // interval of the battery voltage reading
#define ADC_VOLTAGE_CUTOFF 1
#define MIN_TUNE_TIME 30000 // value in micro seconds
#define MAX_UDP_CLIENTS 5
#define MAX_TCP_CLIENTS 5
#define MAX_LAPS_NUM 100 // Maximum number of supported laps per pilot
// 800 and 2700 are about average min max raw values
#define RSSI_ADC_READING_MAX 2700
#define RSSI_ADC_READING_MIN 800
// defines the time after which the crash loop detection assumes the operation is stable
#define CRASH_COUNT_RESET_TIME_MS 300000

#define MAX_NUM_RECEIVERS 6
#define MAX_NUM_PILOTS 8
#define MULTIPLEX_STAY_TIME_US (5 * 1000)

#ifdef USE_DEBUG_OUTPUT
  #define OUTPUT_DEBUG
  #define INPUT_DEBUG
#endif

//#define DEBUG_FILTER // uncomment to constantly print out the raw and filtered data of pilot 1
#define DEBUG_SIGNAL_LOG // uncomment to print out raw adc data from pilot 1 when finishing a lap. debug only!! about 1 secs of 6khz data

// Use the memory we have ;) should be sufficient for around 1sec of full data
// any more and the web ui won't work anymore due to heavy use of dynamic allocation
//#define DEBUG_SIGNAL_LOG_SIZE 3000
//#define DEBUG_SIGNAL_LOG_NUM 2 // number of pilots to track

#include "targets/target.h" // Needs to be at the bottom

#ifndef EEPROM_DEFAULT_MIN_VOLTAGE_MODULE
#define EEPROM_DEFAULT_MIN_VOLTAGE_MODULE 0000
#endif

//#define USE_LOW_POWER // this saves about 5-10mA but the tune time of the module is worse

void InitHardwarePins();
extern uint8_t CS_PINS[MAX_NUM_RECEIVERS];
