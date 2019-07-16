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
#include "Queue.h"

#include "Filter.h"

static Adafruit_INA219 ina219; // A0+A1=GND

static uint32_t LastADCcall;

static esp_adc_cal_characteristics_t adc_chars;

static unsigned int VbatReadingSmooth;
static uint16_t adcLoopCounter = 0;

static float VBATcalibration;
static float mAReadingFloat;
static float VbatReadingFloat;

// count of current active pilots
static uint8_t current_pilot_num = 0;

#define PILOT_FILTER_NUM 1

static lowpass_filter_t adc_voltage_filter;

enum pilot_state {
	PILOT_UNUSED,
	PILOT_ACTIVE,
	PILOT_TAKEN_BY_MODULE,
};

typedef struct pilot_data_s {
	uint16_t RSSIthreshold;
	uint16_t ADCReadingRAW;
	uint16_t ADCvalue;
	lowpass_filter_t filter[PILOT_FILTER_NUM];
	/// Pilot state 0: unsused, 1: active, 2: taken by a module
	uint8_t state;
	uint8_t number;
	uint32_t unused_time; // used to force a certain stay time. if we allow an instant switch, modules would be constantly switching with e.g. 6 modules and 7 pilots
} pilot_data_t;

typedef struct receiver_data_s {
	uint32_t last_hop;
	uint8_t current_pilot;
} receiver_data_t;

static pilot_data_t pilots[MAX_NUM_PILOTS];
static receiver_data_t receivers[MAX_NUM_RECEIVERS];

// multiplexing queue
static queue_t pilot_queue;
static pilot_data_t* pilot_queue_data[MAX_NUM_PILOTS];

static uint16_t multisample_adc1(adc1_channel_t channel, uint8_t samples) {
  uint32_t val = 0;
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
	pilot_data_t* new_pilot = (pilot_data_t*)queue_peek(&pilot_queue);
	if(new_pilot) {
		if(new_pilot->state == PILOT_ACTIVE) {
			if((micros() - new_pilot->unused_time) > MULTIPLEX_STAY_TIME_US + MIN_TUNE_TIME_US){
				new_pilot = (pilot_data_t*)queue_dequeue(&pilot_queue);
				// set old pilot to active again
				if(pilots[receivers[adc].current_pilot].state != PILOT_UNUSED) {
					pilots[receivers[adc].current_pilot].state = PILOT_ACTIVE;
					// readd to multiplex queue
					queue_enqueue(&pilot_queue, pilots + receivers[adc].current_pilot);
					pilots[receivers[adc].current_pilot].unused_time = micros();
				}
				new_pilot->state = PILOT_TAKEN_BY_MODULE;
				receivers[adc].current_pilot = new_pilot->number;
				return true;
			}
		}
		else { // pilot went inactive
			new_pilot = (pilot_data_t*)queue_dequeue(&pilot_queue); // Dequeue inactive pilot
		}
	}
	// Do nothing it the pilot is not active
	return false;
}

void ConfigureADC() {
	memset(pilots, 0, MAX_NUM_PILOTS * sizeof(pilot_data_t));
	for(int i = 0; i < MAX_NUM_PILOTS; ++i) {
		pilots[i].number = i;
	}
	memset(receivers, 0, MAX_NUM_RECEIVERS * sizeof(receiver_data_t));
	pilot_queue.curr_size = 0;
	pilot_queue.max_size = MAX_NUM_PILOTS;
	pilot_queue.data = (void**)pilot_queue_data;

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
	
	for(uint8_t i = 0; i < MAX_NUM_PILOTS; ++i) {
		for(uint8_t j = 0; j < PILOT_FILTER_NUM; ++j) {
			filter_init(&pilots[i].filter[j], cutoff, 166 * getNumReceivers() * 1e-6);
		}
	}
	filter_init(&adc_voltage_filter, ADC_VOLTAGE_CUTOFF, 0);
	
	// By default enable getNumReceivers() pilots
	for(uint8_t i = 0; i < getNumReceivers() && i < MAX_NUM_PILOTS; ++i)  {
		setPilotActive(i, true);
	}
	
}

adc1_channel_t IRAM_ATTR getADCChannel(uint8_t adc_num) {
	adc1_channel_t channel = ADC1;
	switch (adc_num) {
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
	return channel;
}

void IRAM_ATTR nbADCread( void * pvParameters ) {
	static uint8_t current_adc = 0;
	uint32_t now = micros();
	LastADCcall = now;


	if(now - receivers[current_adc].last_hop > MULTIPLEX_STAY_TIME_US + MIN_TUNE_TIME_US) {
		if(setNextPilot(current_adc)) {
			// TODO: add class between this and rx5808
			// TODO: add better multiplexing. Maybe based on the last laptime?
			setModuleChannelBand(getRXChannelPilot(receivers[current_adc].current_pilot), getRXBandPilot(receivers[current_adc].current_pilot), current_adc);
			receivers[current_adc].last_hop = now;
		}
	}


	// go to next adc if vrx is not ready
	if(isRxReady(current_adc)) {
		adc1_channel_t channel = getADCChannel(current_adc);
		pilot_data_t* current_pilot = &pilots[receivers[current_adc].current_pilot];
		if(LIKELY(isInRaceMode())) {
			current_pilot->ADCReadingRAW = adc1_get_raw(channel);
		} else {
			// multisample when not in race mode (for threshold calibration etc)
			current_pilot->ADCReadingRAW = multisample_adc1(channel, 10);
		}

		// Applying calibration
		if (LIKELY(!isCalibrating())) {
			uint16_t rawRSSI = constrain(current_pilot->ADCReadingRAW, EepromSettings.RxCalibrationMin[current_adc], EepromSettings.RxCalibrationMax[current_adc]);
			current_pilot->ADCReadingRAW = map(rawRSSI, EepromSettings.RxCalibrationMin[current_adc], EepromSettings.RxCalibrationMax[current_adc], RSSI_ADC_READING_MIN, RSSI_ADC_READING_MAX);
		}
		
		current_pilot->ADCvalue = current_pilot->ADCReadingRAW;
		for(uint8_t j = 0; j < PILOT_FILTER_NUM; ++j) {
			current_pilot->ADCvalue = filter_add_value(&(current_pilot->filter[j]), current_pilot->ADCvalue);
		}

#ifdef DEBUG_FILTER
		if(receivers[current_adc].current_pilot == 0) {
			Serial.print(current_pilot->ADCReadingRAW);
			Serial.print("\t");
			Serial.println(current_pilot->ADCvalue);
		}
#endif // DEBUG_FILTER
		if (LIKELY(isInRaceMode() > 0)) {
			CheckRSSIthresholdExceeded(receivers[current_adc].current_pilot);
		}
		
		if(current_pilot->number == 0) ++adcLoopCounter;
	} // end if isRxReady
	current_adc = (current_adc + 1) % getNumReceivers();
}


void ReadVBAT_INA219() {
  setVbatFloat(ina219.getBusVoltage_V() + (ina219.getShuntVoltage_mV() / 1000));
  mAReadingFloat = ina219.getCurrent_mA();
}

void IRAM_ATTR CheckRSSIthresholdExceeded(uint8_t pilot) {
	uint32_t CurrTime = millis();
	static uint16_t max_adc = 0;
	static uint32_t max_time = 0;
	if ( pilots[pilot].ADCvalue > pilots[pilot].RSSIthreshold) {
		if (CurrTime > (getMinLapTime() + getLaptime(pilot))) {
			if(pilot == 0) { // enable threshold detection for pilot 0 only for now. makes comparison easier
				if(pilots[pilot].ADCvalue > max_adc) {
					max_adc = pilots[pilot].ADCvalue;
					max_time = CurrTime;
				}
			} else {
				addLap(pilot, CurrTime);
			}
		}
	} else if(pilot == 0 && max_adc && max_time) { // falling edge
		addLap(pilot, max_time);
		max_adc = 0;
		max_time = 0;
	}
}

uint16_t getRSSI(uint8_t pilot) {
  if(pilot < MAX_NUM_PILOTS) {
    return pilots[pilot].ADCvalue;
  }
  return 0;
}

void setRSSIThreshold(uint8_t node, uint16_t threshold) {
  if(node < MAX_NUM_PILOTS) {
    pilots[node].RSSIthreshold = threshold;
  }
}

uint16_t getRSSIThreshold(uint8_t node){
	return pilots[node].RSSIthreshold;
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

float getVbatFloat(bool force_read){
  static uint32_t last_voltage_update = 0;
  if((millis() - last_voltage_update) > VOLTAGE_UPDATE_INTERVAL_MS || force_read) {
    switch (getADCVBATmode()) {
      case ADC_CH5:
        VbatReadingSmooth = esp_adc_cal_raw_to_voltage(adc1_get_raw(ADC5), &adc_chars);
        setVbatFloat(VbatReadingSmooth / 1000.0 * VBATcalibration);
        break;
      case ADC_CH6:
        VbatReadingSmooth = esp_adc_cal_raw_to_voltage(adc1_get_raw(ADC6), &adc_chars);
        setVbatFloat(VbatReadingSmooth / 1000.0 * VBATcalibration);
        break;
      case INA219:
        ReadVBAT_INA219();
      default:
        break;
    }
    last_voltage_update = millis();
    VbatReadingFloat = filter_add_value(&adc_voltage_filter, VbatReadingFloat);
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

uint8_t getActivePilots() {
	return current_pilot_num;
}

bool isPilotActive(uint8_t pilot) {
	return pilots[pilot].state != PILOT_UNUSED;
}

void setPilotActive(uint8_t pilot, bool active) {
	// First we power up all modules. If we have less pilots than modules they get activated at a later stage. As this function will never be called during a race this should be okay
	// There might be a way better solution, but this will have to suffice for now
	// XXX: We have to reset the module since it won't come online with a simple power up
	RXResetAll();
	if(active) {
		if(pilots[pilot].state == PILOT_UNUSED) {
			// If the pilot was unused until now, we set them to active and add them to the queue
			pilots[pilot].state = PILOT_ACTIVE;
			++current_pilot_num;
			queue_enqueue(&pilot_queue, pilots + pilot);
		}
	} else {
		if(pilots[pilot].state != PILOT_UNUSED) {
			pilots[pilot].state = PILOT_UNUSED;
			// Set adc values to 0 for display etc
			pilots[pilot].ADCvalue = 0;
			pilots[pilot].ADCReadingRAW = 0;
			--current_pilot_num;
			// Since the pilot state is now unused, it won't get added back to the queue
		}
	}

	Serial.print("New pilot num: ");
	Serial.println(current_pilot_num);

	// adjust the filters for all active pilots
	for(uint8_t i = 0; i < MAX_NUM_PILOTS; ++i) {
		if(pilots[i].state != PILOT_UNUSED) {
			uint32_t total_pilot_time_us = ((MULTIPLEX_STAY_TIME_US + MIN_TUNE_TIME_US) * current_pilot_num); // Total time for all pilots
			float on_fraction = MULTIPLEX_STAY_TIME_US / (float)total_pilot_time_us * getNumReceivers(); // on percentage of the pilot
			// special case for non multiplexing
			if(current_pilot_num <= getNumReceivers()) {
				on_fraction = 1.0/current_pilot_num;
			}
			for(uint8_t j = 0; j < PILOT_FILTER_NUM; ++j) {
				filter_adjust_dt(&pilots[i].filter[j], 1.0/(6000.0 * on_fraction)); // set sampling rate
				// when multiplexing we are using the average sampling rate per pilot as a timebase
			}
		}
	}

	// Power down all unused modules
	for(uint8_t i = current_pilot_num; i < getNumReceivers(); ++i) {
		RXPowerDown(i);
	}
}
