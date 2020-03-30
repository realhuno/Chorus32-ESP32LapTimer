#include "Laptime.h"

#include <stdint.h>
#include <string.h>
#include <Arduino.h>

#include "HardwareConfig.h"
#include "settings_eeprom.h"
#include "Comms.h"

static uint32_t LapTimes[MAX_NUM_PILOTS][MAX_LAPS_NUM];
static uint16_t lap_counter[MAX_NUM_PILOTS]; //Keep track of what lap we are up too
static uint16_t lap_offset[MAX_NUM_PILOTS]; // Save lap offset to allow for more laps than MAX_LAPS_NUM
static uint16_t best_lap_num[MAX_NUM_PILOTS];
static uint16_t last_lap_sent[MAX_NUM_PILOTS]; // number of the last lap we sent

static uint32_t MinLapTime = 5000;  //this is in millis
static uint32_t start_time = 0;
static uint8_t count_first_lap = 0; // 0 means start table is before the laptimer, so first lap is not a full-fledged lap (i.e. don't respect min-lap-time for the very first lap)
static uint16_t race_num = 0; // number of races

void resetLaptimes() {
  memset(LapTimes, 0, MAX_NUM_PILOTS * MAX_LAPS_NUM * sizeof(LapTimes[0][0]));
  memset(lap_counter, 0, MAX_NUM_PILOTS * sizeof(lap_counter[0]));
  memset(lap_offset, 0, MAX_NUM_PILOTS * sizeof(lap_offset[0]));
  memset(best_lap_num, 0, MAX_NUM_PILOTS * sizeof(best_lap_num[0]));
  memset(last_lap_sent, 0, MAX_NUM_PILOTS * sizeof(last_lap_sent[0]));
}

void sendNewLaps() {
  for (int i = 0; i < MAX_NUM_PILOTS; ++i) {
    int laps_to_send = lap_counter[i] - last_lap_sent[i];
    if(laps_to_send > 0) {
      for(int j = 0; j < laps_to_send && j <= MAX_LAPS_NUM; ++j) {
        sendLap(lap_counter[i] - j, i);
      }
      last_lap_sent[i] += laps_to_send;
    }
  }
}

uint32_t getLaptime(uint8_t receiver, uint8_t lap) {
  if(receiver < MAX_NUM_PILOTS && lap - lap_offset[receiver] < MAX_LAPS_NUM) {
    return LapTimes[receiver][lap - lap_offset[receiver]];
  }
  return 0;
}


uint32_t getLaptime(uint8_t receiver) {
  return getLaptime(receiver, lap_counter[receiver]);
}

uint32_t getLaptimeRel(uint8_t receiver, uint8_t lap) {
  if(lap == 1) {
    return getLaptime(receiver, lap) - start_time;
  } else if(lap == 0) {
    return 0;
  }
  return getLaptime(receiver, lap) - getLaptime(receiver, lap - 1);
}

uint32_t getLaptimeRelToStart(uint8_t receiver, uint8_t lap) {
  return getLaptime(receiver, lap) - start_time;
}

uint32_t getLaptimeRel(uint8_t receiver) {
  return getLaptimeRel(receiver, lap_counter[receiver]);
}

uint8_t addLap(uint8_t receiver, uint32_t time) {
  ++lap_counter[receiver];
  // check if lap fits. if not we move the whole array and phase out old times
  if(lap_counter[receiver] - lap_offset[receiver] >= MAX_LAPS_NUM) {
    memmove(LapTimes[receiver], LapTimes[receiver] + 10, 10* sizeof(LapTimes[receiver]));
    lap_offset[receiver] += 10;
  }
  LapTimes[receiver][lap_counter[receiver] - lap_offset[receiver]] = time;
  if((getLaptimeRel(receiver) < getLaptimeRel(receiver, best_lap_num[receiver]) || getLaptimeRel(receiver, best_lap_num[receiver]) == 0)) {
    // skip best time if we skip the first lap
    if(!(lap_counter[receiver] == 1 && !count_first_lap)) {
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
  ++race_num;
}

void setCountFirstLap(uint8_t shouldWaitForFirstLap) {
  count_first_lap = shouldWaitForFirstLap;
}

uint8_t getCountFirstLap() {
  return count_first_lap;
}

uint16_t getRaceNum() {
  return race_num;
}
