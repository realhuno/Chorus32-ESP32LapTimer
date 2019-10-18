#ifndef __BEEPER_H__
#define __BEEPER_H__

#include <stdint.h>

#include "Queue.h"

typedef struct pattern_s{
  uint16_t on_time;
  uint16_t off_time;
  uint8_t repeat;
} pattern_t;

const pattern_t BEEP_SINGLE = {50, 50, 1};
const pattern_t BEEP_DOUBLE = {50, 50, 2};
const pattern_t BEEP_FIVE = {50, 50, 5};
const pattern_t BEEP_CHIRPS = {10, 10, 6};

void beeper_add_to_queue(const pattern_t* pattern = &BEEP_SINGLE);
void beeperUpdate();
void beeper_init();


#endif // __BEEPER_H__
