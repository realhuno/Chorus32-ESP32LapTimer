#include "Laptime.h"

#include <stdint.h>

#include "HardwareConfig.h"
#include "settings_eeprom.h"

static uint32_t LapTimes[MAX_NUM_PILOTS][MAX_LAPS_NUM];
static uint8_t lap_counter[MAX_NUM_PILOTS]; //Keep track of what lap we are up too
static uint8_t best_lap_num[MAX_NUM_PILOTS];
static int last_lap_sent[MAX_NUM_PILOTS];

static uint32_t MinLapTime = 5000;  //this is in millis
static uint32_t start_time = 0;

void resetLaptimes() {
	memset(LapTimes, 0, MAX_NUM_PILOTS * MAX_LAPS_NUM * sizeof(LapTimes[0][0]));
	memset(lap_counter, 0, MAX_NUM_PILOTS * sizeof(lap_counter[0]));
	memset(best_lap_num, 0, MAX_NUM_PILOTS * sizeof(best_lap_num[0]));
	memset(last_lap_sent, 0, MAX_NUM_PILOTS * sizeof(last_lap_sent[0]));
}

void sendNewLaps() {
  for (int i = 0; i < MAX_NUM_PILOTS; ++i) {
    int laps_to_send = lap_counter[i] - last_lap_sent[i];
    if(laps_to_send > 0) {
      for(int j = 0; j < laps_to_send; ++j) {
        sendLap(lap_counter[i] - j, i);
      }
      last_lap_sent[i] += laps_to_send;
    }
  }
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
  return getLaptime(receiver, lap_counter[receiver]);
}

uint8_t addLap(uint8_t receiver, uint32_t time) {
	lap_counter[receiver] = lap_counter[receiver] + 1;
	LapTimes[receiver][lap_counter[receiver]] = time;
	Serial.print("New lap: ");
	Serial.print(getLaptimeRel(receiver));
	Serial.print(" Best: ");
	Serial.println(getLaptimeRel(receiver, best_lap_num[receiver]));
	if((getLaptimeRel(receiver) > getLaptimeRel(receiver, best_lap_num[receiver]) || getLaptimeRel(receiver, best_lap_num[receiver]) == 0)) {
		// skip best time if we skip the first lap
		if(!(lap_counter[receiver] == 1 && skip_first_lap)) {
			best_lap_num[receiver] = lap_counter[receiver];
		}
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
