#include "Calibration.h"

#include "ADC.h"
#include "HardwareConfig.h"
#include "RX5808.h"
#include "settings_eeprom.h"
#include "OLED.h"
#include "Timer.h"
#include "Utils.h"
#include "Comms.h"

static int calibrationFreqIndex = 0;
static bool isCurrentlyCalibrating = false;
static Timer calibrationTimer = Timer(50);

bool isCalibrating() {
  return isCurrentlyCalibrating;
}

void rssiCalibration() {
  for (uint8_t i = 0; i < getNumReceivers(); i++) {
    EepromSettings.RxCalibrationMin[i] = 5000;
    EepromSettings.RxCalibrationMax[i] = 0;
  }

  isCurrentlyCalibrating = true;
  calibrationFreqIndex = 0;
  setModuleFrequencyAll(channelFreqTable[calibrationFreqIndex]);
  calibrationTimer.reset();
  setDisplayScreenNumber(3);
}

void rssiCalibrationUpdate() {
  if (UNLIKELY(isCurrentlyCalibrating && calibrationTimer.hasTicked())) {
    for (uint8_t i = 0; i < getNumReceivers(); i++) {
      while(!isRxReady(i)); // Wait for rx to become ready
      adc1_channel_t channel = getADCChannel(i);
      uint16_t value = multisample_adc1(channel, 10);
      if (value < EepromSettings.RxCalibrationMin[i])
        EepromSettings.RxCalibrationMin[i] = value;

      if (value > EepromSettings.RxCalibrationMax[i])
        EepromSettings.RxCalibrationMax[i] = value;
    }
    calibrationFreqIndex++;
    if (calibrationFreqIndex < 8*8) { // 8*8 = 8 bands * 8 channels = total number of freq in channelFreqTable.
      setModuleFrequencyAll(channelFreqTable[calibrationFreqIndex]);
      calibrationTimer.reset();

    } else {
      for (int i = 0; i < getNumReceivers(); i++) {
        setModuleChannelBand(i);
        // Prevent min > max
        EepromSettings.RxCalibrationMax[i] = MAX(EepromSettings.RxCalibrationMax[i], EepromSettings.RxCalibrationMin[i] + 1);
      }
      isCurrentlyCalibrating = false;
      setSaveRequired();
      setDisplayScreenNumber(0);
      sendCalibrationFinished();
    }
  }
}

int getcalibrationFreqIndex() {
  return calibrationFreqIndex;
}
