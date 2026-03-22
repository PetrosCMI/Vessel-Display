#include "boat_data.h"

BoatData          gBoat;
SemaphoreHandle_t gBoatMutex = NULL;
volatile bool gResetDepthMinMax = false;
