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
#include "UDP.h"
#include "Calibration.h"
#include "Laptime.h"

static Timer ina219Timer = Timer(1000);

static Adafruit_INA219 ina219; // A0+A1=GND

static uint32_t ADCstartMicros;
static uint32_t ADCfinishMicros;
static uint16_t ADCcaptime;

static uint32_t LastADCcall;

static hw_timer_t * timer = NULL;
static portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

static SemaphoreHandle_t xBinarySemaphore;

static esp_adc_cal_characteristics_t adc_chars;

static int RSSIthresholds[MaxNumRecievers];
static uint16_t ADCReadingsRAW[MaxNumRecievers];
static unsigned int VbatReadingRaw;
static unsigned int VbatReadingSmooth;
static int FilteredADCvalues[MaxNumRecievers];
static int ADCvalues[MaxNumRecievers];
static uint16_t adcLoopCounter = 0;

static FilterBeLp2_10HZ Filter_10HZ[6] = {FilterBeLp2_10HZ(), FilterBeLp2_10HZ(), FilterBeLp2_10HZ(), FilterBeLp2_10HZ(), FilterBeLp2_10HZ(), FilterBeLp2_10HZ()};
static FilterBeLp2_20HZ Filter_20HZ[6] = {FilterBeLp2_20HZ(), FilterBeLp2_20HZ(), FilterBeLp2_20HZ(), FilterBeLp2_20HZ(), FilterBeLp2_20HZ(), FilterBeLp2_20HZ()};
static FilterBeLp2_50HZ Filter_50HZ[6] = {FilterBeLp2_50HZ(), FilterBeLp2_50HZ(), FilterBeLp2_50HZ(), FilterBeLp2_50HZ(), FilterBeLp2_50HZ(), FilterBeLp2_50HZ()};
static FilterBeLp2_100HZ Filter_100HZ[6] = {FilterBeLp2_100HZ(), FilterBeLp2_100HZ(), FilterBeLp2_100HZ(), FilterBeLp2_100HZ(), FilterBeLp2_100HZ(), FilterBeLp2_100HZ()};

static float VBATcalibration;
static float mAReadingFloat;
static float VbatReadingFloat;

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

}

void IRAM_ATTR nbADCread( void * pvParameters ) {

  Serial.println("Task created...");

  while (1) {

    xSemaphoreTake( xBinarySemaphore, portMAX_DELAY );
    uint32_t now = micros();
    //Serial.print(now - LastADCcall);
    //Serial.print(",");
    ADCstartMicros = now;
    LastADCcall = now;

    ADCReadingsRAW[0] = adc1_get_raw(ADC1);
    ADCReadingsRAW[1] = adc1_get_raw(ADC2);
    ADCReadingsRAW[2] = adc1_get_raw(ADC3);
    ADCReadingsRAW[3] = adc1_get_raw(ADC4);
    ADCReadingsRAW[4] = adc1_get_raw(ADC5);
    ADCReadingsRAW[5] = adc1_get_raw(ADC6);
    

    // Applying calibration
    if (!isCalibrating()) {
      for (uint8_t i = 0; i < NumRecievers; i++) {
        if((getADCVBATmode() == ADC_CH5 && i == 4) || (getADCVBATmode() == ADC_CH6 && i == 5)) continue; // skip if voltage is on this channel
        uint16_t rawRSSI = constrain(ADCReadingsRAW[i], EepromSettings.RxCalibrationMin[i], EepromSettings.RxCalibrationMax[i]);
        ADCReadingsRAW[i] = map(rawRSSI, EepromSettings.RxCalibrationMin[i], EepromSettings.RxCalibrationMax[i], 800, 2700); // 800 and 2700 are about average min max raw values
      }
    }
    
    switch (getRXADCfilter()) {

      case LPF_10Hz:
        for (int i = 0; i < 6; i++) {
          ADCvalues[i] = Filter_10HZ[i].step(ADCReadingsRAW[i]);
        }
        break;

      case LPF_20Hz:
        for (int i = 0; i < 6; i++) {
          ADCvalues[i] = Filter_20HZ[i].step(ADCReadingsRAW[i]);
        }
        break;

      case LPF_50Hz:
        for (int i = 0; i < 6; i++) {
          ADCvalues[i] = Filter_50HZ[i].step(ADCReadingsRAW[i]);
        }
        break;

      case LPF_100Hz:
        for (int i = 0; i < 6; i++) {
          ADCvalues[i] = Filter_100HZ[i].step(ADCReadingsRAW[i]);
        }
        break;
    }

    switch (getADCVBATmode()) {
      case ADC_CH5:
        VbatReadingSmooth = esp_adc_cal_raw_to_voltage(ADCvalues[4], &adc_chars);
        setVbatFloat(VbatReadingSmooth / 1000.0 * VBATcalibration);
        break;
      case ADC_CH6:
        VbatReadingSmooth = esp_adc_cal_raw_to_voltage(ADCvalues[5], &adc_chars);
       setVbatFloat(VbatReadingSmooth / 1000.0 * VBATcalibration);
        break;
    }

    if (isInRaceMode() > 0) {
      CheckRSSIthresholdExceeded();
    }


    //ADCcaptime = micros() - ADCstartMicros;
    // Serial.println(ADCcaptime);
  }
}


void StartNB_ADCread() {
  Serial.println("Starting ADC reading task on core 1");
  xTaskCreatePinnedToCore(
    nbADCread,   /* Function to implement the task */
    "ADCreader", /* Name of the task */
    10000,      /* Stack size in words */
    NULL,       /* Task input parameter */
    -1,          /* Priority of the task */
    NULL,
    0);       /* Task handle. */
  //1);  /* Core where the task should run */
}


void ReadVBAT_INA219() {
  if (ina219Timer.hasTicked()) {
    setVbatFloat(ina219.getBusVoltage_V() + (ina219.getShuntVoltage_mV() / 1000));
    mAReadingFloat = ina219.getCurrent_mA();
    ina219Timer.reset();
  }
}

void IRAM_ATTR readADCs() {
  ++adcLoopCounter;
  
  static BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  /* un-block the interrupt processing task now */
  xSemaphoreGiveFromISR( xBinarySemaphore, &xHigherPriorityTaskWoken );
  if ( xHigherPriorityTaskWoken) {
    portYIELD_FROM_ISR(); // this wakes up sample_timer_task immediately
  }
}

void IRAM_ATTR CheckRSSIthresholdExceeded() {
  uint32_t CurrTime = millis();
  for (uint8_t i = 0; i < NumRecievers; i++) {
    if ( ADCvalues[i] > RSSIthresholds[i]) {
		if (CurrTime > (getMinLapTime() + getLaptime(i))) {
        uint8_t lap_num = addLap(i, CurrTime);
        sendLap(lap_num, i);
      }
    }
  }
}


void InitADCtimer() {
  xBinarySemaphore = xSemaphoreCreateBinary();
  StartNB_ADCread();

  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &readADCs, true);
  timerAlarmWrite(timer, 1000, true);
  timerAlarmEnable(timer);
}

void StopADCtimer() {
  //timerAttachInterrupt(timer, NULL, true); //This doesn't work, not sure why.
}

uint16_t getRSSI(uint8_t index) {
  if(index < MaxNumRecievers) {
    return ADCvalues[index];  
  }
  return 0;
}

void setRSSIThreshold(uint8_t node, uint16_t threshold) {
  if(node < MaxNumRecievers) {
    RSSIthresholds[node] = threshold;
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
