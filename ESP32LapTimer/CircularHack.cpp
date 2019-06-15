#include "CircularHack.h"

static float VbatReadingFloat;

float getVbatFloat(){
	return VbatReadingFloat;
}

void setVbatFloat(float val){
	VbatReadingFloat = val;
}
