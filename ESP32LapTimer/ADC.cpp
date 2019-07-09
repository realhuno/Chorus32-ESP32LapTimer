////Functions to Read RSSI from ADCs//////
#include <driver/adc.h>
#include <driver/timer.h>
#include <esp_adc_cal.h>

#include <Wire.h>
#include <Adafruit_INA219.h>

#include "HardwareConfig.h"
#include "Comms.h"
#include "settings_eeprom.h"
#include "ADC.h"
#include "Timer.h"
#include "Output.h"
#include "Calibration.h"
#include "Laptime.h"
#include "Utils.h"
#include "RX5808.h"

#include "Filter.h"


static Timer ina219Timer = Timer(1000);

static Adafruit_INA219 ina219; // A0+A1=GND

static uint32_t LastADCcall;

static esp_adc_cal_characteristics_t adc_chars;

static int RSSIthresholds[MaxNumRecievers];
static uint16_t ADCReadingsRAW[MaxNumRecievers];
static unsigned int VbatReadingSmooth;
static int ADCvalues[MaxNumRecievers];
static uint16_t adcLoopCounter = 0;

static float VBATcalibration;
static float mAReadingFloat;
static float VbatReadingFloat;

/// Pilot state 0: unsused, 1: active, 2: taken by a module
static uint8_t active_pilots[MAX_NUM_PILOTS];
static uint8_t current_pilot_num = 0;
static uint32_t last_hop[MaxNumRecievers];
static uint8_t current_pilot[MaxNumRecievers];

#define FILTER_NUM 2

static lowpass_filter_t filter[MaxNumRecievers][FILTER_NUM];
static lowpass_filter_t adc_voltage_filter;

static uint16_t multisample_adc1(adc1_channel_t channel, uint8_t samples) {
	uint16_t val = 0;
	for(uint8_t i = 0; i < samples; ++i) {
		val += adc1_get_raw(channel);
	}
	return val/samples;
}

/**
 * Find next free pilot and set them to busy. Marks the old pilot as active. Sets the current pilot for the given module
 * \returns if a different pilot was found.
 */
static bool setNextPilot(uint8_t adc) {
	for(uint8_t i = 1; i < MaxNumRecievers + 1; ++i) {
		uint8_t next_pilot = (current_pilot[adc] + i) % MaxNumRecievers;
		if(active_pilots[next_pilot] == 1) {
			active_pilots[current_pilot[adc]] = 1;
			active_pilots[next_pilot] = 2;
			current_pilot[adc] = next_pilot;
			return true;
		}
	}
	return false;
}

static void setOneToOnePilotAssignment() {
	if(current_pilot_num <= NUM_PHYSICAL_RECEIVERS) {
		for(uint8_t i = 0; i < current_pilot_num; ++i) {
			setNextPilot(i);
			// We are assuming adcs are always in order without gaps
			setModuleChannelBand(getRXChannelPilot(current_pilot[i]), getRXBandPilot(current_pilot[i]), i);
		}
	}
}

void ConfigureADC() {

  adc1_config_width(ADC_WIDTH_BIT_12);

  adc1_config_channel_atten(ADC1, ADC_ATTEN_6db);
  adc1_config_channel_atten(ADC2, ADC_ATTEN_6db);
  adc1_config_channel_atten(ADC3, ADC_ATTEN_6db);
  adc1_config_channel_atten(ADC4, ADC_ATTEN_6db);
  adc1_config_channel_atten(ADC5, ADC_ATTEN_6db);
  adc1_config_channel_atten(ADC6, ADC_ATTEN_6db);

  //since the reference voltage can range from 1000mV to 1200mV we are using 1100mV as a default
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_6db, ADC_WIDTH_BIT_12, 1100, &adc_chars);

  ina219.begin();
  ReadVBAT_INA219();
  float cutoff = 0;
  switch (getRXADCfilter()) {
	case LPF_10Hz:
		cutoff = 10;
		break;

	case LPF_20Hz:
		cutoff = 20;
		break;

	case LPF_50Hz:
		cutoff = 50;
		break;

	case LPF_100Hz:
		cutoff = 100;
		break;
	}
	for(uint8_t i = 0; i < MaxNumRecievers; ++i) {
		for(uint8_t j = 0; j < FILTER_NUM; ++j) {
			filter_init(&filter[i][j], cutoff);
		}
	}
	
	memset(active_pilots, 0, MAX_NUM_PILOTS);
	memset(last_hop, 0, MAX_NUM_RECEIVERS);
	memset(current_pilot, 0, MAX_NUM_RECEIVERS);
	filter_init(&adc_voltage_filter, ADC_VOLTAGE_CUTOFF);
}

void IRAM_ATTR nbADCread( void * pvParameters ) {
	static uint8_t current_adc = 0;
	uint32_t now = micros();
	LastADCcall = now;
	adc1_channel_t channel = ADC1;
	switch (current_adc) {
		case 0:
		channel = ADC1;
		break;
		case 1:
		channel = ADC2;
		break;
		case 2:
		channel = ADC3;
		break;
		case 3:
		channel = ADC4;
		break;
		case 4:
		channel = ADC5;
		break;
		case 5:
		channel = ADC6;
		break;
	}
	// Only multiplex, if we need to
	if(current_pilot_num > NUM_PHYSICAL_RECEIVERS) {
		if(now - last_hop[current_adc] > MULTIPLEX_STAY_TIME_US + MIN_TUNE_TIME_US) {
			setNextPilot(current_adc);
			// TODO: add class between this and rx5808
			// TODO: add better multiplexing. Maybe based on the last laptime?
			// TODO: add smarter jumping with multiple modules available
			setModuleChannelBand(getRXChannelPilot(current_pilot[current_adc]), getRXBandPilot(current_pilot[current_adc]), current_adc);
			last_hop[current_adc] = now;
		}
	}
	// go to next adc if vrx is not ready
	if(isRxReady(current_adc)) {
		if(LIKELY(isInRaceMode())) {
			ADCReadingsRAW[current_pilot[current_adc]] = adc1_get_raw(channel);
		} else {
			// multisample when not in race mode (for threshold calibration etc)
			ADCReadingsRAW[current_pilot[current_adc]] = multisample_adc1(channel, 10);
		}

		// Applying calibration
		if (LIKELY(!isCalibrating())) {
			uint16_t rawRSSI = constrain(ADCReadingsRAW[current_pilot[current_adc]], EepromSettings.RxCalibrationMin[current_adc], EepromSettings.RxCalibrationMax[current_adc]);
			ADCReadingsRAW[current_pilot[current_adc]] = map(rawRSSI, EepromSettings.RxCalibrationMin[current_adc], EepromSettings.RxCalibrationMax[current_adc], 800, 2700); // 800 and 2700 are about average min max raw values
		}
		
		ADCvalues[current_pilot[current_adc]] = ADCReadingsRAW[current_pilot[current_adc]];
		for(uint8_t j = 0; j < FILTER_NUM; ++j) {
			filter_add_value(&filter[current_pilot[current_adc]][j], ADCvalues[current_pilot[current_adc]]);
			ADCvalues[current_pilot[current_adc]] = filter[current_pilot[current_adc]][j].state;
		}

		if (LIKELY(isInRaceMode() > 0)) {
			CheckRSSIthresholdExceeded(current_pilot[current_adc]);
		}
		
		if(current_adc == 0) ++adcLoopCounter;
	} // end if isRxReady
	current_adc = (current_adc + 1) % NUM_PHYSICAL_RECEIVERS;
}


void ReadVBAT_INA219() {
  if (ina219Timer.hasTicked()) {
    setVbatFloat(ina219.getBusVoltage_V() + (ina219.getShuntVoltage_mV() / 1000));
    mAReadingFloat = ina219.getCurrent_mA();
    ina219Timer.reset();
  }
}

void IRAM_ATTR CheckRSSIthresholdExceeded(uint8_t node) {
	uint32_t CurrTime = millis();
	if ( ADCvalues[node] > RSSIthresholds[node]) {
		if (CurrTime > (getMinLapTime() + getLaptime(node))) {
			uint8_t lap_num = addLap(node, CurrTime);
			sendLap(lap_num, node);
		}
	}
}

uint16_t getRSSI(uint8_t index) {
  if(index < MaxNumRecievers) {
    return ADCvalues[index];
  }
  return 0;
}

void updatePilotNumbers() {
	memset(active_pilots, 1, MaxNumRecievers);
	current_pilot_num = MaxNumRecievers;
	for(uint8_t i = 0; i < MaxNumRecievers; ++i) {
		if(RSSIthresholds[i] == 12) { // equals to 1 in the app
			active_pilots[i] = 0;
			// Set adc values to 0 for display etc
			ADCvalues[i] = 0;
			ADCReadingsRAW[i] = 0;
			--current_pilot_num;
		}
	}
	Serial.print("New pilot num: ");
	Serial.println(current_pilot_num);
	
	if(current_pilot_num <= NUM_PHYSICAL_RECEIVERS) {
		Serial.println("Multiplexing disabled.");
		setOneToOnePilotAssignment();
	}
}

void setRSSIThreshold(uint8_t node, uint16_t threshold) {
  if(node < MaxNumRecievers) {
    RSSIthresholds[node] = threshold;
    updatePilotNumbers();
  }
}

uint16_t getRSSIThreshold(uint8_t node){
	return RSSIthresholds[node];
}

uint16_t getADCLoopCount() {
	return adcLoopCounter;
}

void setADCLoopCount(uint16_t count) {
	adcLoopCounter = count;
}

void setVbatCal(float calibration) {
	VBATcalibration = calibration;
}

float getMaFloat() {
	return mAReadingFloat;
}

float getVbatFloat(){
	static uint32_t last_voltage_update = 0;
	if((micros() - last_voltage_update) > VOLTAGE_UPDATE_INTERVAL_US) {
		uint8_t multisampling_samples = 1;
		if(LIKELY(!isInRaceMode())) {
			multisampling_samples = VOLTAGE_MULTISAMPLING_SAMPLES;
		}
		switch (getADCVBATmode()) {
			case ADC_CH5:
				VbatReadingSmooth = esp_adc_cal_raw_to_voltage(multisample_adc1(ADC5, multisampling_samples), &adc_chars);
				setVbatFloat(VbatReadingSmooth / 1000.0 * VBATcalibration);
				break;
			case ADC_CH6:
				VbatReadingSmooth = esp_adc_cal_raw_to_voltage(multisample_adc1(ADC6, multisampling_samples), &adc_chars);
				setVbatFloat(VbatReadingSmooth / 1000.0 * VBATcalibration);
				break;
			case INA219:
				ReadVBAT_INA219();
			default:
				break;
		}
		VbatReadingFloat = filter_add_value(&adc_voltage_filter, VbatReadingFloat);
		last_voltage_update = micros();
	}
	return VbatReadingFloat;
}

void setVbatFloat(float val){
	VbatReadingFloat = val;
}

void setVBATcalibration(float val) {
  VBATcalibration = val;
}

float getVBATcalibration() {
  return VBATcalibration;
}
