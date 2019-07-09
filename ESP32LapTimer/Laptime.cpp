#include "Laptime.h"

#include <stdint.h>

#include "HardwareConfig.h"

extern uint8_t NumRecievers;

static uint32_t LapTimes[MAX_NUM_PILOTS][MAX_LAPS_NUM];
static uint8_t lap_counter[MAX_NUM_PILOTS]; //Keep track of what lap we are up too
static uint8_t best_lap_num[MAX_NUM_PILOTS];

static uint32_t MinLapTime = 5000;  //this is in millis
static uint32_t start_time = 0;

void resetLaptimes() {
	memset(LapTimes, 0, MAX_NUM_PILOTS * MAX_LAPS_NUM * sizeof(LapTimes[0][0]));
	memset(lap_counter, 0, MAX_NUM_PILOTS * sizeof(lap_counter[0]));
	memset(best_lap_num, 0, MAX_NUM_PILOTS * sizeof(best_lap_num[0]));
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
  return getLaptime(receiver, lap_counter[receiver]);
}



uint8_t addLap(uint8_t receiver, uint32_t time) {
	lap_counter[receiver] = lap_counter[receiver] + 1;
	LapTimes[receiver][lap_counter[receiver]] = time;
	if(getLaptimeRel(receiver) > getLaptimeRel(receiver, best_lap_num[receiver])) {
		best_lap_num[receiver] = lap_counter[receiver];
	}
	return lap_counter[receiver];
}

uint8_t getBestLap(uint8_t pilot) {
	return best_lap_num[pilot];
}

uint32_t getMinLapTime() {
  return MinLapTime;
}

void setMinLapTime(uint32_t time) {
  MinLapTime = time;
}

uint8_t getCurrentLap(uint8_t receiver) {
  return lap_counter[receiver];
}

void startRaceLap() {
  resetLaptimes();
  start_time = millis();
}
