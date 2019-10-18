#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>

#include <esp_task_wdt.h>
#include <rom/rtc.h>

#include "Comms.h"
#include "ADC.h"
#include "HardwareConfig.h"
#include "RX5808.h"
#include "Bluetooth.h"
#include "settings_eeprom.h"
#ifdef OLED
#include "OLED.h"
#endif
#include "TimerWebServer.h"
#include "Beeper.h"
#include "Calibration.h"
#include "Output.h"
#ifdef USE_BUTTONS
#include "Buttons.h"
#endif
#include "TimerWebServer.h"
#include "Watchdog.h"
#include "Utils.h"
#include "Laptime.h"
#include "Wireless.h"

#include "CrashDetection.h"
#ifdef USE_ARDUINO_OTA
#include <ArduinoOTA.h>
#endif

//#define BluetoothEnabled //uncomment this to use bluetooth (experimental, ble + wifi appears to cause issues)

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
    if(LIKELY(!isCalibrating())) {
      nbADCread(NULL);
    } else {
      rssiCalibrationUpdate();
    }
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
  init_crash_detection();


  Serial.begin(115200);
  Serial.println("Booting....");
#ifdef USE_ARDUINO_OTA
  if(is_crash_mode()) {
    log_e("Detected crashing. Starting ArduinoOTA only!");
    InitWifiAP();
    ArduinoOTA.begin();
    return;
  }
#endif

  InitSPI();
  InitHardwarePins();
  RXPowerDownAll(); // Powers down all RX5808's

#ifdef OLED
  oledSetup();
#endif

  bool all_modules_off = false;
  if(rtc_get_reset_reason(0) == 15 || rtc_get_reset_reason(1) == 15) {
    all_modules_off = true;
    Serial.println("Rebooted from brownout...disabling all modules...");
  }
#ifdef USE_BUTTONS
  newButtonSetup();
#endif
  resetLaptimes();
  beeper_init();

  EepromSettings.setup();
  setRXADCfilterCutoff(EepromSettings.RXADCfilterCutoff);
  setADCVBATmode(EepromSettings.ADCVBATmode);
  setVbatCal(EepromSettings.VBATcalibration);

  for(int i = 0; i < MAX_NUM_PILOTS; ++i) {
    setRXBandPilot(i, EepromSettings.RXBand[i]);
    setRXChannelPilot(i, EepromSettings.RXChannel[i]);
  }
  delay(30);
  ConfigureADC(all_modules_off);
  delay(250);

  InitWifi();

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

  xTaskCreatePinnedToCore(adc_task, "ADCreader", 4096, NULL, 1, &adc_task_handle, 0);
  hw_timer_t* adc_task_timer = timerBegin(0, 8, true);
  timerAttachInterrupt(adc_task_timer, &adc_read, true);
  timerAlarmWrite(adc_task_timer, 1667, true); // 6khz -> 1khz per adc channel
  timerAlarmEnable(adc_task_timer);

  xTaskCreatePinnedToCore(eeprom_task, "eepromSave", 4096, NULL, tskIDLE_PRIORITY, NULL, 1); 
}

void loop() {
#ifdef USE_ARDUINO_OTA
  ArduinoOTA.handle();
  if(is_crash_mode()) return;
#endif
  if(millis() > CRASH_COUNT_RESET_TIME_MS) {
    reset_crash_count();
  }
  rssiCalibrationUpdate();
#ifdef USE_BUTTONS
  newButtonUpdate();
#endif
#ifdef OLED
  // We need to pause the OLED during update otherwise we crash due to I2C
  if(!isUpdating()) {
    OLED_CheckIfUpdateReq();
  }
#endif
  sendNewLaps();
  update_outputs();
  SendCurrRSSIloop();

#ifdef WIFI_MODE_ACCESSPOINT
  handleDNSRequests();
#endif

  handleNewHTTPClients();

  beeperUpdate();
  if(UNLIKELY(!isInRaceMode())) {
    thresholdModeStep();
  }
}
