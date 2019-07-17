#pragma once 

#include "HardwareConfig.h"
#include "crc.h"

enum ADCVBATmode_ {OFF, ADC_CH5, ADC_CH6, INA219};

#define MaxChannel 7
#define MaxBand 7

#define MaxFreq 5945
#define MinFreq 5180

#define MaxVbatMode 3
#define MaxVBATCalibration 100.00
#define MaxThreshold 4095

struct EepromSettingsStruct {
  uint16_t eepromVersionNumber;
  uint8_t RXBand[MAX_NUM_PILOTS];
  uint8_t RXChannel[MAX_NUM_PILOTS];
  uint16_t RSSIthresholds[MAX_NUM_PILOTS];
  uint16_t RXADCfilterCutoff;
  ADCVBATmode_ ADCVBATmode;
  float VBATcalibration;
  uint8_t NumReceivers;
  uint16_t RxCalibrationMin[MAX_NUM_RECEIVERS];
  uint16_t RxCalibrationMax[MAX_NUM_RECEIVERS];
  uint8_t WiFiProtocol; // 0 is b only, 1 is bgn
  uint8_t WiFiChannel;
  uint16_t min_voltage_module; // voltage in millivolts where all modules are disabled on startup (use case: powering a fully populated board using usb)
  uint32_t display_timeout_ms;
  crc_t crc; // This MUST be the last variable!


  void setup();
  void load();
  void save();
  void defaults();
  bool SanityCheck();
  void updateCRC();
  bool validateCRC();
  crc_t calcCRC();
};

extern EepromSettingsStruct EepromSettings;

ADCVBATmode_ getADCVBATmode();
uint16_t getRXADCfilterCutoff();

void setRXADCfilterCutoff(uint16_t cutoff);
void setADCVBATmode(ADCVBATmode_ mode);

int getWiFiChannel();
int getWiFiProtocol();
uint16_t getMinVoltageModule();

uint8_t getNumReceivers();
uint32_t getDisplayTimeout();

void setSaveRequired();
