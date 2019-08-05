#pragma once

#include <esp_attr.h>
#include <driver/adc.h>
#include <stdint.h>

#include "HardwareConfig.h"
#include "Filter.h"

void ConfigureADC(bool disable_all_modules);
void IRAM_ATTR CheckRSSIthresholdExceeded(uint8_t node);
void ReadVBAT_INA219();
void IRAM_ATTR nbADCread( void * pvParameters );
adc1_channel_t IRAM_ATTR getADCChannel(uint8_t adc_num);
uint16_t multisample_adc1(adc1_channel_t channel, uint8_t samples);

uint16_t getRSSI(uint8_t index);
void setRSSIThreshold(uint8_t node, uint16_t threshold);
uint16_t getRSSIThreshold(uint8_t node);
uint16_t getADCLoopCount();
void setADCLoopCount(uint16_t count);

void setVbatCal(float calibration);
float getMaFloat();

float getVbatFloat(bool force_read = false);
void setVbatFloat(float val);

float getVBATcalibration();
void setVBATcalibration(float val);

// TODO: these don't belong here!
uint8_t getActivePilots();
bool isPilotActive(uint8_t pilot);
void setPilotActive(uint8_t pilot, bool active);
void setPilotFilters(uint16_t cutoff);

void setPilotBand(uint8_t pilot, uint8_t band);
void setPilotChannel(uint8_t pilot, uint8_t channel);

bool isPilotMultiplexOff(uint8_t pilot);
void setilotMultiplexOff(uint8_t pilot, bool off);
void setPilotFilters(uint16_t cutoff);

void setPilotBand(uint8_t pilot, uint8_t band);
void setPilotChannel(uint8_t pilot, uint8_t channel);

