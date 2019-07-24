#include "Filter.h"

#include <Arduino.h>

#ifndef MATH_PI
#define MATH_PI 3.14159
#endif

void filter_init(lowpass_filter_t* filter, float cutoff, float dt) {
  filter->RC = 1 / (2 * MATH_PI * cutoff);
  filter->alpha = dt / (filter->RC + dt);
}
float IRAM_ATTR filter_add_value(lowpass_filter_t* filter, float value, bool dynamic_dt) {
  if(dynamic_dt) {
    uint32_t now = micros();
    float dt = (now - filter->last_call) * 1e-6f;
    filter->alpha = dt / (filter->RC + dt);
    filter->last_call = now;
  }
  // y[i] := y[i-1] + α * (x[i] - y[i-1])
  filter->state =  filter->state + filter->alpha * (value - filter->state);
  return filter->state;
}

void filter_adjust_dt(lowpass_filter_t* filter, float dt) {
  filter->alpha = dt / (filter->RC + dt);
}
