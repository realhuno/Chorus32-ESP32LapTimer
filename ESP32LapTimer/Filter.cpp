#include "Filter.h"

#include <Arduino.h>

#ifndef MATH_PI
#define MATH_PI 3.14159
#endif

void filter_init(lowpass_filter_t* filter, float cutoff) {
	filter->RC = 1 / (2 * MATH_PI * cutoff);
}
void filter_add_value(lowpass_filter_t* filter, float value) {
	uint32_t now = micros();
	float dt = (now - filter->last_call) * 1e-6f;
	filter->alpha = dt / (filter->RC + dt);
	// y[i] := y[i-1] + α * (x[i] - y[i-1])
	filter->state =  filter->state + filter->alpha * (value - filter->state);
	filter->last_call = now;
}
