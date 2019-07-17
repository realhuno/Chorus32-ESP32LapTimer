#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>

#include <esp_task_wdt.h>

#include "Comms.h"
#include "ADC.h"
#include "HardwareConfig.h"
#include "RX5808.h"
#include "Bluetooth.h"
#include "settings_eeprom.h"
#ifdef OLED
#include "OLED.h"
#endif
#include "WebServer.h"
#include "Beeper.h"
#include "Calibration.h"
#include "Output.h"
#ifdef USE_BUTTONS
#include "Buttons.h"
#endif
#include "WebServer.h"
#include "Watchdog.h"
#include "Utils.h"
#include "Laptime.h"
#include "Utils.h"

//#define BluetoothEnabled //uncomment this to use bluetooth (experimental, ble + wifi appears to cause issues)

//
#define MAX_SRV_CLIENTS 5
WiFiClient serverClients[MAX_SRV_CLIENTS];

static TaskHandle_t adc_task_handle = NULL;

void IRAM_ATTR adc_read() {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  /* un-block the interrupt processing task now */
  vTaskNotifyGiveFromISR(adc_task_handle, &xHigherPriorityTaskWoken);
  if(xHigherPriorityTaskWoken) {
    portYIELD_FROM_ISR();
  }
}

void IRAM_ATTR adc_task(void* args) {
  watchdog_add_task();
  while(42) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    nbADCread(NULL);
    watchdog_feed();
  }
}

void eeprom_task(void* args) {
	const TickType_t xDelay = EEPROM_COMMIT_DELAY_MS / portTICK_PERIOD_MS;
	while(42) {
		EepromSettings.save();
		vTaskDelay(xDelay);
	}
}


void setup() {

#ifdef OLED
  oledSetup();
#endif

  Serial.begin(230400);
  Serial.println("Booting....");
#ifdef USE_BUTTONS
  newButtonSetup();
#endif
  resetLaptimes();

  EepromSettings.setup();
  setRXADCfilterCutoff(EepromSettings.RXADCfilterCutoff);
  setADCVBATmode(EepromSettings.ADCVBATmode);
  setVbatCal(EepromSettings.VBATcalibration);
  for(int i = 0; i < MAX_NUM_PILOTS; ++i) {
    setRXBandPilot(i, EepromSettings.RXBand[i]);
    setRXChannelPilot(i, EepromSettings.RXChannel[i]);
  }

  delay(500);
  InitHardwarePins();
  ConfigureADC();

  InitSPI();
  RXPowerDownAll(); // Powers down all RX5808's
  delay(250);

  InitWifiAP();

  InitWebServer();

  if (!EepromSettings.SanityCheck()) {
    EepromSettings.defaults();
    Serial.println("Detected That EEPROM corruption has occured.... \n Resetting EEPROM to Defaults....");
  }

  commsSetup();

  for (int i = 0; i < MAX_NUM_PILOTS; i++) {
    setRSSIThreshold(i, EepromSettings.RSSIthresholds[i]);
  }

  // inits modules with defaults.  Loops 10 times  because some Rx modules dont initiate correctly.
  for (int i = 0; i < getNumReceivers()*10; i++) {
    setModuleChannelBand(i % getNumReceivers());
    delayMicroseconds(MIN_TUNE_TIME_US);
  }
	
	init_outputs();
	Serial.println("Starting ADC reading task on core 0");
	adc_semaphore = xSemaphoreCreateBinary();

  xTaskCreatePinnedToCore(adc_task, "ADCreader", 4096, NULL, 1, &adc_task_handle, 0);
  hw_timer_t* adc_task_timer = timerBegin(0, 8, true);
  timerAttachInterrupt(adc_task_timer, &adc_read, true);
  timerAlarmWrite(adc_task_timer, 1667, true); // 6khz -> 1khz per adc channel
  timerAlarmEnable(adc_task_timer);

  xTaskCreatePinnedToCore(eeprom_task, "eepromSave", 4096, NULL, tskIDLE_PRIORITY, NULL, 1); 
}

void loop() {
  rssiCalibrationUpdate();
#ifdef USE_BUTTONS
  newButtonUpdate();
#endif
#ifdef OLED
  OLED_CheckIfUpdateReq();
#endif
  sendNewLaps();
  update_outputs();
  SendCurrRSSIloop();
  updateWifi();

  beeperUpdate();
  if(UNLIKELY(!isInRaceMode())) {
    thresholdModeStep();
  }
}
