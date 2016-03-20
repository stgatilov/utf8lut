#pragma once

#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>
#include "Base/i386_timer.h"

#define TIMING_SLOTS(X) \
	X(DECODE, 1) \
	X(ENCODE, 2) \
	X(TEMP, 3) \
	X(MAX, 4)

//======================================================

#define CONCAT(x, y) x##y

#define TIMING_X_DEFINE(name, idx) \
	static const int CONCAT(TIMING_, name) = idx;
TIMING_SLOTS(TIMING_X_DEFINE);

struct TimingData {
	uint64_t totalTime[TIMING_MAX];
	uint64_t totalElems[TIMING_MAX];
	uint64_t startTime[TIMING_MAX];
};

extern TimingData timingData;

//======================================================

#define TIMING_START(name) { \
	int slot = CONCAT(TIMING_, name); \
	uint64_t &startTime = timingData.startTime[slot]; \
	assert(startTime == 0); \
	startTime = read_cycle_counter(); \
}

#define TIMING_END(name, elems) { \
	uint64_t endTime = read_cycle_counter(); \
	int slot = CONCAT(TIMING_, name); \
	uint64_t &startTime = timingData.startTime[slot]; \
	assert(startTime != 0); \
	timingData.totalTime[slot] += endTime - startTime; \
	timingData.totalElems[slot] += uint64_t(elems); \
	startTime = 0; \
}

inline void TimingPrintAll() {
	#define TIMING_X_PRINT(name, idx) \
		printf("slot %d %10s : %6.3lf cyc/el  %12" PRIu64 " elems\n", \
			idx, #name, \
			timingData.totalTime[idx] / (timingData.totalElems[idx] + 1e-9), \
			timingData.totalElems[idx] \
		);
	TIMING_SLOTS(TIMING_X_PRINT);
}
#define TIMING_PRINT() TimingPrintAll();
