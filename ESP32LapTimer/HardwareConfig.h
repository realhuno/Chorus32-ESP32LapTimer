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

#define MaxNumRecievers 6
#define MAX_NUM_RECEIVERS MaxNumRecievers
#define MAX_NUM_PILOTS MaxNumRecievers
#define NUM_PHYSICAL_RECEIVERS 1
#define MULTIPLEX_STAY_TIME_US (5 * 1000)

#define ADC_VOLTAGE_CUTOFF 1
#define VOLTAGE_UPDATE_INTERVAL_US (1000 * 1000)
#define VOLTAGE_MULTISAMPLING_SAMPLES 10

#define MIN_TUNE_TIME_US 30000 // value in micro seconds

#define MAX_LAPS_NUM 100

#define OLED //uncomment this to enable OLED support

#define MAX_UDP_CLIENTS 5

//#define USE_BLUETOOTH // Disabled by default. If you enable it you might need to change the partition scheme to "Huge APP"

#include "targets/target.h" // Needs to be at the bottom

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
