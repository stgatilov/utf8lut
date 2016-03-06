#pragma once

#include "Base/BOM_Profiler.h"

extern BOM_Table *timingData;

#define TIMING_START(name) { \
	if (!timingData) timingData = init_BOM_timer(); \
	start_BOM_interval(timingData); \
}

#define TIMING_END(name, elems) { \
	end_BOM_interval(timingData, elems); \
}

#define TIMING_PRINT() { \
	if (timingData) dump_BOM_table(timingData); \
}
