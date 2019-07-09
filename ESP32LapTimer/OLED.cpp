#include "OLED.h"

#include <Wire.h>
#include "SSD1306.h"
#include "Font.h"
#include "Timer.h"
#include "Screensaver.h"
#include "ADC.h"
#include "settings_eeprom.h"
#include "RX5808.h"
#include "Calibration.h"
#include "WebServer.h"
#include "Filter.h"

extern uint8_t NumRecievers;

#define SUMMARY_PILOTS_PER_PAGE 6

static uint8_t oledRefreshTime = 50;

static Timer oledTimer = Timer(oledRefreshTime);

static SSD1306 display(0x3c, 21, 22);  // 21 and 22 are default pins

typedef struct oled_page_s {
  void* data;
  void (*init)(void* data);
  void (*draw_page)(void* data);
  void (*process_input)(void* data, uint8_t index, uint8_t type);
} oled_page_t;

static struct rxPageData_s {
  uint8_t currentPilotNumber;
} rxPageData;

static struct adcPageData_s {
  lowpass_filter_t filter;
} adcPageData;

static struct summaryPageData_s {
  uint8_t first_pilot;
} summaryPageData;


oled_page_t oled_pages[] = {
  {&summaryPageData, summary_page_init, summary_page_update, summary_page_input},
  {&adcPageData, adc_page_init, adc_page_update, next_page_input},
  {NULL, NULL, calib_page_update, next_page_input},
  {NULL, NULL, airplane_page_update, airplane_page_input},
  {&rxPageData, rx_page_init, rx_page_update, rx_page_input}
};

#define NUM_OLED_PAGES (sizeof(oled_pages)/sizeof(oled_pages[0]))

static uint8_t current_page = 0;

static void oledNextPage() {
  current_page = (current_page + 1) % NUM_OLED_PAGES;
}

void oledSetup(void) {
  display.init();
  display.flipScreenVertically();
  display.clear();
  display.drawFastImage(0, 0, 128, 64, ChorusLaptimerLogo_Screensaver);
  display.display();
  display.setFont(Dialog_plain_9);
  
  for(uint8_t i = 0; i < NUM_OLED_PAGES; ++i) {
    if(oled_pages[i].init) {
      oled_pages[i].init(oled_pages[i].data);
    }
  }
}

void OLED_CheckIfUpdateReq() {
  if (oledTimer.hasTicked()) {
    if(oled_pages[current_page].draw_page) {
      display.clear();
      oled_pages[current_page].draw_page(oled_pages[current_page].data);
      display.display();
    }
    oledTimer.reset();
  }
}

void oledInjectInput(uint8_t index, uint8_t type) {
  if(oled_pages[current_page].process_input) {
    oled_pages[current_page].process_input(oled_pages[current_page].data, index, type);
  }
}

void next_page_input(void* data, uint8_t index, uint8_t type) {
  (void)data;
  if(index == 0 && type == BUTTON_SHORT) {
    oledNextPage();
  }
}

void rx_page_init(void* data) {
  rxPageData_s* my_data = (rxPageData_s*) data;
  my_data->currentPilotNumber = 0;
}

void rx_page_input(void* data, uint8_t index, uint8_t type) {
  rxPageData_s* my_data = (rxPageData_s*) data;
  if(index == 0 && type == BUTTON_SHORT) {
    ++my_data->currentPilotNumber;
    if(my_data->currentPilotNumber >= MAX_NUM_PILOTS) {
      oledNextPage();
      my_data->currentPilotNumber = 0;
    }
  }
  else if(index == 1 && type == BUTTON_SHORT) {
    incrementRxFrequency(my_data->currentPilotNumber);
  }
  else if(index == 1 && type == BUTTON_LONG) {
    incrementRxBand(my_data->currentPilotNumber);
  }
}

void rx_page_update(void* data) {
  // Gather Data
  rxPageData_s* my_data = (rxPageData_s*) data;
  uint8_t frequencyIndex = getRXChannelPilot(my_data->currentPilotNumber) + (8 * getRXBandPilot(my_data->currentPilotNumber));
  uint16_t frequency = channelFreqTable[frequencyIndex];

  // Display things
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 0, "Settings for RX" + String(my_data->currentPilotNumber + 1));
  display.drawString(0, 18, getBandLabel(getRXBandPilot(my_data->currentPilotNumber)) + String(getRXChannelPilot(my_data->currentPilotNumber) + 1) + " - " + frequency);
  if (getRSSI(my_data->currentPilotNumber) < 600) {
    display.drawProgressBar(48, 35, 120 - 42, 8, map(600, 600, 3500, 0, 85));
  } else {
    display.drawProgressBar(48, 35, 120 - 42, 8, map(getRSSI(my_data->currentPilotNumber), 600, 3500, 0, 85));
  }
  display.setFont(Dialog_plain_9);
  display.drawString(0,35, "RSSI: " + String(getRSSI(my_data->currentPilotNumber) / 12));
  display.drawVerticalLine(45 + map(getRSSIThreshold(my_data->currentPilotNumber), 600, 3500, 0, 85),  35, 8); // line to show the RSSIthresholds
  display.drawString(0,46, "Btn2 SHORT - Channel.");
  display.drawString(0,55, "Btn2 LONG  - Band.");
}

void summary_page_init(void* data) {
  summaryPageData_s* my_data = (summaryPageData_s*) data;
  my_data->first_pilot = 0;
}

void summary_page_input(void* data, uint8_t index, uint8_t type) {
  summaryPageData_s* my_data = (summaryPageData_s*) data;
  if(index == 1 && type == BUTTON_SHORT) {
    my_data->first_pilot += SUMMARY_PILOTS_PER_PAGE;
    if(my_data->first_pilot >= getActivePilots()) {
      my_data->first_pilot = 0;
    }
  }
  else {
    next_page_input(data, index, type);
  }
}

void summary_page_update(void* data) {
  summaryPageData_s* my_data = (summaryPageData_s*) data;
  // Display on time
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  // Hours
  if (millis() / 3600000 < 10) {
    display.drawString(0, 0, "0" + String(millis() / 3600000) + ":");
  } else {
    display.drawString(0, 0, String(millis() / 3600000) + ":");
  }
  // Mins
  if (millis() % 3600000 / 60000 < 10) {
    display.drawString(18, 0, "0" + String(millis() % 3600000 / 60000) + ":");
  } else {
    display.drawString(18, 0, String(millis() % 3600000 / 60000) + ":");
  }
  // Seconds
  if (millis() % 60000 / 1000 < 10) {
    display.drawString(36, 0, "0" + String(millis() % 60000 / 1000));
  } else {
    display.drawString(36, 0, String(millis() % 60000 / 1000));
  }

  // Voltage
  if (getADCVBATmode() != 0) {
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.drawString(127, 0, String(getVbatFloat(), 2) + "V");
  }

  if (getADCVBATmode() == INA219) {
    display.drawString(90, 0, String(getMaFloat()/1000, 2) + "A");
  }
  
  // Rx modules
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  uint8_t skipped = 0;
  uint8_t first_pilot = my_data->first_pilot;
  for (uint8_t i = 0; i < SUMMARY_PILOTS_PER_PAGE + skipped && (i + first_pilot) < MAX_NUM_PILOTS; ++i) {
    if(isPilotActive(i)) {
      display.drawString(0, 9 + (i - skipped) * 9, getBandLabel(getRXBandPilot(i + first_pilot)) + String(getRXChannelPilot(i + first_pilot) + 1) + ", " + String(getRSSI(i + first_pilot) / 12));
      if (getRSSI(i + first_pilot) < 600) {
        display.drawProgressBar(40, 10 + (i - skipped) * 9, 127 - 42, 8, map(600, 600, 3500, 0, 85));
      } else {
        display.drawProgressBar(40, 10 + (i - skipped) * 9, 127 - 42, 8, map(getRSSI(i + first_pilot), 600, 3500, 0, 85));
      }
      display.drawVerticalLine(40 + map(getRSSIThreshold(i + first_pilot), 600, 3500, 0, 85),  10 + (i - skipped) * 9, 8); // line to show the RSSIthresholds
    }
    else {
      ++skipped;
    }
  }
}

void adc_page_init(void* data) {
  adcPageData_s* my_data = (adcPageData_s*)data;
  filter_init(&my_data->filter, 1, 1); 
}

void adc_page_update(void* data) {
  adcPageData_s* my_data = (adcPageData_s*)data;
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  float freq = filter_add_value(&my_data->filter, getADCLoopCount() * (1000.0 / oledRefreshTime), true);
  display.drawString(0, 0, "ADC loop " + String(freq) + " Hz");
  setADCLoopCount(0);
  display.drawString(0, 9, String(getMaFloat()) + " mA");
}

void calib_page_update(void* data) {
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 0, "Frequency - " + String(channelFreqTable[getcalibrationFreqIndex()]) + "Hz");
  display.drawString(0,  9, "Min = " + String(EepromSettings.RxCalibrationMin[0]) + ", Max = " + String(EepromSettings.RxCalibrationMax[0]));
  display.drawString(0, 18, "Min = " + String(EepromSettings.RxCalibrationMin[1]) + ", Max = " + String(EepromSettings.RxCalibrationMax[1]));
  display.drawString(0, 27, "Min = " + String(EepromSettings.RxCalibrationMin[2]) + ", Max = " + String(EepromSettings.RxCalibrationMax[2]));
  display.drawString(0, 36, "Min = " + String(EepromSettings.RxCalibrationMin[3]) + ", Max = " + String(EepromSettings.RxCalibrationMax[3]));
  display.drawString(0, 45, "Min = " + String(EepromSettings.RxCalibrationMin[4]) + ", Max = " + String(EepromSettings.RxCalibrationMax[4]));
  display.drawString(0, 54, "Min = " + String(EepromSettings.RxCalibrationMin[5]) + ", Max = " + String(EepromSettings.RxCalibrationMax[5]));
}

void airplane_page_update(void* data) {
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 0, "Airplane Mode Settings:");
  display.drawString(0, 15, "Long Press Button 2 to");
  display.drawString(0, 26, "toggle Airplane mode.");
  if (!isAirplaneModeOn()) {
    display.drawString(0, 42, "Airplane Mode: OFF");
    display.drawString(0, 51, "WiFi: ON  | Draw: " + String(getMaFloat()/1000, 2) + "A");
  } else {
    display.drawString(0, 42, "Airplane Mode: ON");
    display.drawString(0, 51, "WiFi: OFF  | Draw: " + String(getMaFloat()/1000, 2) + "A");
  }
}

void airplane_page_input(void* data, uint8_t index, uint8_t type) {
  if(index == 1 && type == BUTTON_LONG) {
    toggleAirplaneMode();
  } else {
    next_page_input(data, index, type);
  }
}

void incrementRxFrequency(uint8_t currentRXNumber) {
  uint8_t currentRXChannel = getRXChannelPilot(currentRXNumber);
  uint8_t currentRXBand = getRXBandPilot(currentRXNumber);
  currentRXChannel++;
  if (currentRXChannel >= 8) {
    //currentRXBand++;
    currentRXChannel = 0;
  }
  if (currentRXBand >= 7 && currentRXChannel >= 2) {
    currentRXBand = 0;
    currentRXChannel = 0;
  }
  setModuleChannelBand(currentRXChannel,currentRXBand,currentRXNumber);
}
void incrementRxBand(uint8_t currentRXNumber) {
  uint8_t currentRXChannel = getRXChannelPilot(currentRXNumber);
  uint8_t currentRXBand = getRXBandPilot(currentRXNumber);
  currentRXBand++;
  if (currentRXBand >= 8) {
    currentRXBand = 0;
  }
  setModuleChannelBand(currentRXChannel,currentRXBand,currentRXNumber);
}

void setDisplayScreenNumber(uint16_t num) {
  if(num < NUM_OLED_PAGES) {
    current_page = num;
  }
}

uint16_t getDisplayScreenNumber() {
  return current_page;
}

