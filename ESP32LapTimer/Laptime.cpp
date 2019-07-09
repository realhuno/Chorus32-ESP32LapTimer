#include "Laptime.h"

#include <stdint.h>
#include <Arduino.h>

#include "HardwareConfig.h"

extern uint8_t NumRecievers;

static uint32_t LapTimes[MAX_NUM_PILOTS][MAX_LAPS_NUM];
static uint8_t LapTimePtr[MAX_NUM_PILOTS]; //Keep track of what lap we are up too
static uint8_t best_lap_num[MAX_NUM_PILOTS];

static uint32_t MinLapTime = 5000;  //this is in millis
static uint32_t start_time = 0;
static uint8_t skip_first_lap = 0; // 0 means start table is before the laptimer, so first lap is not a full-fledged lap (i.e. don't respect min-lap-time for the very first lap)

void resetLaptimes() {
	memset(LapTimes, 0, MAX_NUM_PILOTS * MAX_LAPS_NUM * sizeof(LapTimes[0][0]));
	memset(LapTimePtr, 0, MAX_NUM_PILOTS * sizeof(LapTimePtr[0]));
	memset(best_lap_num, 0, MAX_NUM_PILOTS * sizeof(best_lap_num[0]));
}

uint32_t getLaptime(uint8_t receiver, uint8_t lap) {
	return LapTimes[receiver][lap];
}

uint32_t getLaptimeRel(uint8_t receiver, uint8_t lap) {
	if(lap == 1) {
		return getLaptime(receiver, lap) - start_time;
	}
	return getLaptime(receiver, lap) - getLaptime(receiver, lap - 1);
}

uint32_t getLaptimeRelToStart(uint8_t receiver, uint8_t lap) {
	return getLaptime(receiver, lap) - start_time;
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
	Serial.print("New lap: ");
	Serial.print(getLaptimeRel(receiver));
	Serial.print(" Best: ");
	Serial.println(getLaptimeRel(receiver, best_lap_num[receiver]));
	if((getLaptimeRel(receiver) > getLaptimeRel(receiver, best_lap_num[receiver]) || getLaptimeRel(receiver, best_lap_num[receiver]) == 0)) {
		// skip best time if we skip the first lap
		if(!(LapTimePtr[receiver] == 1 && skip_first_lap)) {
			best_lap_num[receiver] = LapTimePtr[receiver];
		}
	}
	return LapTimePtr[receiver];
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
	return LapTimePtr[receiver];
}

void startRaceLap() {
	start_time = millis();
}

void setSkipFirstLap(uint8_t shouldWaitForFirstLap) {
	skip_first_lap = shouldWaitForFirstLap;
}

uint8_t getSkipFirstLap() {
	return skip_first_lap;
}
