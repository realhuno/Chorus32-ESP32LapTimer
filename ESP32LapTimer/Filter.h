#ifndef __FILTER_H__
#define __FILTER_H__

#include <stdint.h>

// see https://en.wikipedia.org/wiki/Low-pass_filter#Simple_infinite_impulse_response_filter for reference
typedef struct lowpass_filter_s {
  float state;
  float RC;
  float alpha;
  uint32_t last_call;
  
} lowpass_filter_t; 

void filter_init(lowpass_filter_t* filter, float cutoff);
float filter_add_value(lowpass_filter_t* filter, float value);


#endif // __FILTER_H__
