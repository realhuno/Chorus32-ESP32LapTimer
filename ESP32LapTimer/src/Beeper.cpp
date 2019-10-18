#include "Beeper.h"

#include "Timer.h"
#include "HardwareConfig.h"

#include <Arduino.h>

#define BEEP_MAX_QUEUE 4

static queue_t beep_queue;
static pattern_t* beep_queue_data[BEEP_MAX_QUEUE];

static pattern_t* current_pattern = NULL;
static uint32_t start_time = 0;
static uint8_t repeat_count = 0;

void beeper_add_to_queue(const pattern_t* pattern) {
  queue_enqueue(&beep_queue, (void*)pattern);
}

void beeperUpdate() {
  // currently playing one pattern
  if(current_pattern) {
    if(millis() - start_time < current_pattern->on_time) {
      digitalWrite(BEEPER, HIGH);
    }
    else if (millis() - start_time < current_pattern->off_time) {
      digitalWrite(BEEPER, LOW);
    }
    else { // pattern end
      ++repeat_count;
      start_time = millis();
      if(repeat_count >= current_pattern->repeat) {
        current_pattern = NULL;
      }
    }
  }
  else {
    // get new pattern
    current_pattern = (pattern_t*)queue_dequeue(&beep_queue);
    start_time = millis();
    repeat_count = 0;
  }
}


void beeper_init() {
  beep_queue.curr_size = 0;
  beep_queue.max_size = BEEP_MAX_QUEUE;
  beep_queue.data = (void**)beep_queue_data;
}
