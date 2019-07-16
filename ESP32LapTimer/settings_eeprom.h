#pragma once 

#include "HardwareConfig.h"
#include "crc.h"

enum RXADCfilter_ {LPF_10Hz, LPF_20Hz, LPF_50Hz, LPF_100Hz};
enum ADCVBATmode_ {OFF, ADC_CH5, ADC_CH6, INA219};

#define MaxChannel 7
#define MaxBand 7

#define MaxFreq 5945
#define MinFreq 5180

#define MaxADCFilter 3
#define MaxVbatMode 3
#define MaxVBATCalibration 100.00
#define MaxThreshold 4095

struct EepromSettingsStruct {
  uint16_t eepromVersionNumber;
  uint8_t RXBand[MAX_NUM_PILOTS];
  uint8_t RXChannel[MAX_NUM_PILOTS];
  uint16_t RSSIthresholds[MAX_NUM_PILOTS];
  RXADCfilter_ RXADCfilter;
  ADCVBATmode_ ADCVBATmode;
  float VBATcalibration;
  uint8_t NumReceivers;
  uint16_t RxCalibrationMin[MAX_NUM_RECEIVERS];
  uint16_t RxCalibrationMax[MAX_NUM_RECEIVERS];
  uint8_t WiFiProtocol; // 0 is b only, 1 is bgn
  uint8_t WiFiChannel;
  uint16_t min_voltage_module; // voltage in millivolts where all modules are disabled on startup (use case: powering a fully populated board using usb)
  crc_t crc;


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

RXADCfilter_ getRXADCfilter();
ADCVBATmode_ getADCVBATmode();

void setRXADCfilter(RXADCfilter_ filter);
void setADCVBATmode(ADCVBATmode_ mode);

int getWiFiChannel();
int getWiFiProtocol();
int getNumReceivers();
uint16_t getMinVoltageModule();

void setSaveRequired();
