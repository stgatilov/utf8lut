#pragma once

#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <inttypes.h>
#include "Base/i386_timer.h"

#define TIMING_SLOTS(X) \
	X(DECODE, 1) \
	X(ENCODE, 2) \
	X(MAX, 3)

//======================================================

#define CONCAT(x, y) x##y

#define TIMING_X_DEFINE(name, idx) \
	static const int CONCAT(TIMING_, name) = idx;
TIMING_SLOTS(TIMING_X_DEFINE);

struct TimingData {
	uint64_t totalTime[TIMING_MAX];
	uint64_t totalElems[TIMING_MAX];
	uint64_t currStartTime;
	int currSlot;
};

extern TimingData timingData;

//======================================================

#define TIMING_START(name) { \
	assert(timingData.currSlot == 0); \
	timingData.currSlot = CONCAT(TIMING_, name); \
	timingData.currStartTime = read_cycle_counter(); \
}

#define TIMING_END(name, elems) { \
	int slot = CONCAT(TIMING_, name); \
	assert(slot == timingData.currSlot); \
	uint64_t endTime = read_cycle_counter(); \
	timingData.totalTime[slot] += endTime - timingData.currStartTime; \
	timingData.totalElems[slot] += uint64_t(elems); \
	timingData.currStartTime = 0; \
	timingData.currSlot = 0; \
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
