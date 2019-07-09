#include "Laptime.h"

#include <stdint.h>
#include <Arduino.h>

#include "HardwareConfig.h"

extern uint8_t NumRecievers;

static volatile uint32_t LapTimes[MAX_NUM_PILOTS][MAX_LAPS_NUM];
static volatile int LapTimePtr[MAX_NUM_PILOTS] = {0, 0, 0, 0, 0, 0}; //Keep track of what lap we are up too

static uint32_t MinLapTime = 5000;  //this is in millis
static uint32_t start_time = 0;

void resetLaptimes() {
  for (int i = 0; i < NumRecievers; ++i) {
    LapTimePtr[i] = 0;
  }
}

uint32_t getLaptime(uint8_t receiver, uint8_t lap) {
  return LapTimes[receiver][lap];
}

uint32_t getLaptimeRel(uint8_t receiver, uint8_t lap) {
	if(lap == 1) {
		return start_time - getLaptime(receiver, lap);
	}
	return getLaptime(receiver, lap - 1) - getLaptime(receiver, lap);
}

uint32_t getLaptimeRelToStart(uint8_t receiver, uint8_t lap) {
	return start_time - getLaptime(receiver, lap);
}

uint32_t getLaptimeRel(uint8_t receiver) {
	return getLaptimeRel(receiver, LapTimePtr[receiver]);
}

uint32_t getLaptime(uint8_t receiver) {
  return getLaptime(receiver, LapTimePtr[receiver]);
}

uint8_t addLap(uint8_t receiver, uint32_t time) {
  LapTimePtr[receiver] = LapTimePtr[receiver] + 1;
  LapTimes[receiver][LapTimePtr[receiver]] = time;
  return LapTimePtr[receiver];
}

uint32_t getMinLapTime() {
  return MinLapTime;
}

void setMinLapTime(uint32_t time) {
  MinLapTime = time;
}

uint8_t getCurrentLap(uint8_t receiver) {
  return LapTimePtr[receiver];
}

void startRaceLap() {
	start_time = millis();
}
