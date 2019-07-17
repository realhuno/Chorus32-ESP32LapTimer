#pragma once
#include <stdint.h>
#include <Arduino.h>

#define BOARD_DEFAULT 1
#define BOARD_OLD 2
#define BOARD_TTGO_LORA 3

void InitHardwarePins();

///Define Pin configuration here, these are the defaults as given on github

#define BOARD BOARD_DEFAULT


// DO NOT CHANGE BELOW UNLESS USING CUSTOM HARDWARE

#define EEPROM_VERSION_NUMBER 7 // Increment when eeprom struct modified
#define EEPROM_COMMIT_DELAY_MS 10000 // the time between checks to save the eeprom in ms

// TODO: add to eeprom?
#define WIFI_AP_NAME "Chorus32 LapTimer"

#define MAX_NUM_RECEIVERS 6
#define MAX_NUM_PILOTS 8
#define MULTIPLEX_STAY_TIME_US (5 * 1000)

#define ADC_VOLTAGE_CUTOFF 1
#define VOLTAGE_UPDATE_INTERVAL_US (1000 * 1000)

#define MIN_TUNE_TIME_US 30000 // value in micro seconds

#define MAX_LAPS_NUM 100

#define OLED //uncomment this to enable OLED support
#define MAX_UDP_CLIENTS 5


#define USE_BLUETOOTH // Disabled by default. If you enable it you might need to change the partition scheme to "Huge APP"
#define USE_SERIAL_OUTPUT

#ifdef USE_DEBUG_OUTPUT
  #define OUTPUT_DEBUG
  #define INPUT_DEBUG
#endif

//#define DEBUG_FILTER // uncomment to constantly print out the raw and filtered data of pilot 1
//#define DEBUG_SIGNAL_LOG // uncomment to print out raw adc data from pilot 1 when finishing a lap. debug only!! about 1 secs of 6khz data

#include "targets/target.h" // Needs to be at the bottom

#ifndef EEPROM_DEFAULT_MIN_VOLTAGE_MODULE
#define EEPROM_DEFAULT_MIN_VOLTAGE_MODULE 6000
#endif

//#define USE_LOW_POWER // this saves about 5-10mA but the tune time of the module is worse

#if defined(BUTTON1) && defined(BUTTON2)
#define USE_BUTTONS
#endif

// Define unconfigured pins
#ifndef SCK
#define SCK -1
#endif
#ifndef MOSI
#define MOSI -1
#endif
#ifndef MISO
#define MISO -1
#endif
