#ifndef __ADC_H__
#define __ADC_H__

#include "HardwareConfig.h"
#include "Filter.h"

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

void ConfigureADC();
void InitADCtimer();
void IRAM_ATTR CheckRSSIthresholdExceeded();
void ReadVBAT_INA219();

uint16_t getRSSI(uint8_t index);
void setRSSIThreshold(uint8_t node, uint16_t threshold);
uint16_t getRSSIThreshold(uint8_t node);
uint16_t getADCLoopCount();
void setADCLoopCount(uint16_t count);

void setVbatCal(float calibration);
float getMaFloat();

#endif
