#ifndef __COMMS_H__
#define __COMMS_H__

#include "HardwareConfig.h"

#include <stdint.h>
#include <esp_attr.h>

void HandleSerialRead();
void HandleServerUDP();
void SendCurrRSSIloop();
void IRAM_ATTR sendLap(uint8_t Lap, uint8_t NodeAddr);
void commsSetup();
void thresholdModeStep();
void handleSerialControlInput(char *controlData, uint8_t  ControlByte, uint8_t NodeAddr, uint8_t length);
bool isInRaceMode();
void startRace();
void stopRace();
bool isExperimentalModeOn();
void sendCalibrationFinished();
void sendExtendedRSSI(uint8_t node, uint32_t time, uint16_t rssi);
void update_comms();

#define TRIGGER_NORMAL 0
#define TRIGGER_PEAK 1
uint8_t get_trigger_mode();

#endif // __COMMS_H__
