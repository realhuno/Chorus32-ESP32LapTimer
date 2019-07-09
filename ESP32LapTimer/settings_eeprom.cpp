#include <EEPROM.h>
#include "settings_eeprom.h"
#include "Comms.h"
#include "crc.h"

struct EepromSettingsStruct EepromSettings;

static bool eepromSaveRequired = false;

void EepromSettingsStruct::setup() {
  EEPROM.begin(512);
  this->load();
}

void EepromSettingsStruct::load() {
  EEPROM.get(0, *this);
  Serial.println("EEPROM LOADED");

  if (this->eepromVersionNumber != EEPROM_VERSION_NUMBER) {
    this->defaults();
    Serial.println("EEPROM DEFAULTS LOADED");
  }
}

bool EepromSettingsStruct::SanityCheck() {

  bool IsGoodEEPROM = true;

  if (EepromSettings.NumReceivers > MAX_NUM_RECEIVERS) {
    IsGoodEEPROM = false;
    Serial.print("Error: Corrupted EEPROM value getNumReceivers(): ");
    Serial.println(EepromSettings.NumReceivers);
  }


  if (EepromSettings.RXADCfilter > MaxADCFilter) {
    IsGoodEEPROM = false;
    Serial.print("Error: Corrupted EEPROM value RXADCfilter: ");
    Serial.println(EepromSettings.RXADCfilter);
  }

  if (EepromSettings.ADCVBATmode > MaxVbatMode) {
    IsGoodEEPROM = false;
    Serial.print("Error: Corrupted EEPROM value ADCVBATmode: ");
    Serial.println(EepromSettings.ADCVBATmode);
  }

  if (EepromSettings.VBATcalibration > MaxVBATCalibration) {
    IsGoodEEPROM = false;
    Serial.print("Error: Corrupted EEPROM value VBATcalibration: ");
    Serial.println(EepromSettings.VBATcalibration);
  }

  for (int i = 0; i < MAX_NUM_PILOTS; i++) {
    if (EepromSettings.RXBand[i] > MaxBand) {
      IsGoodEEPROM = false;
      Serial.print("Error: Corrupted EEPROM NODE: ");
      Serial.print(i);
      Serial.print(" value MaxBand: ");
      Serial.println(EepromSettings.RXBand[i]);
    }

  }

  for (int i = 0; i < MAX_NUM_PILOTS; i++) {
    if (EepromSettings.RXChannel[i] > MaxChannel) {
      IsGoodEEPROM = false;
      Serial.print("Error: Corrupted EEPROM NODE: ");
      Serial.print(i);
      Serial.print(" value RXChannel: ");
      Serial.println(EepromSettings.RXChannel[i]);
    }
  }

  for (int i = 0; i < MAX_NUM_PILOTS; i++) {
    if (EepromSettings.RSSIthresholds[i] > MaxThreshold) {
      IsGoodEEPROM = false;
      Serial.print("Error: Corrupted EEPROM NODE: ");
      Serial.print(i);
      Serial.print(" value RSSIthresholds: ");
      Serial.println(EepromSettings.RSSIthresholds[i]);
    }
  }
  return IsGoodEEPROM && this->validateCRC();
}

void EepromSettingsStruct::save() {
  if (eepromSaveRequired) {
	this->updateCRC();
    EEPROM.put(0, *this);
    EEPROM.commit();
    eepromSaveRequired = false;
    Serial.println("EEPROM SAVED");
  }
}

void EepromSettingsStruct::defaults() {
	memset(this, 0, sizeof(EepromSettingsStruct));
	for(uint8_t i = 0; i < MAX_NUM_RECEIVERS; ++i){
		this->RxCalibrationMax[i] = 2700;
		this->RxCalibrationMin[i] = 800;
	}
	
	for(uint8_t i = 0; i < MAX_NUM_PILOTS; ++i){
		this->RSSIthresholds[i] = 150 * 12;
	}
	this->eepromVersionNumber = EEPROM_VERSION_NUMBER;
	this->ADCVBATmode = INA219;
	this->RXADCfilter = LPF_20Hz;
	this->VBATcalibration = 1;
	this->NumReceivers = 6;

	this->updateCRC();
	EEPROM.put(0, *this);
	EEPROM.commit();
}

crc_t EepromSettingsStruct::calcCRC() {
	crc_t crc = crc_init();
	crc = crc_update(crc, this, sizeof(*this) - sizeof(this->crc));
	crc = crc_finalize(crc);
	return crc;
}

void EepromSettingsStruct::updateCRC() {
	this->crc = this->calcCRC();
}

bool EepromSettingsStruct::validateCRC(){
	return this->crc == this->calcCRC();
}


RXADCfilter_ getRXADCfilter() {
	return EepromSettings.RXADCfilter;
}

ADCVBATmode_ getADCVBATmode() {
	return EepromSettings.ADCVBATmode;
}

void setRXADCfilter(RXADCfilter_ filter) {
	EepromSettings.RXADCfilter = filter;
}

void setADCVBATmode(ADCVBATmode_ mode) {
	EepromSettings.ADCVBATmode = mode;
}

void setSaveRequired() {
	eepromSaveRequired = true;
}

int getWiFiChannel(){
  return EepromSettings.WiFiChannel;
}
int getWiFiProtocol(){
  return EepromSettings.WiFiProtocol;
}

int getNumReceivers() {
	return EepromSettings.NumReceivers;
}
