#pragma once

#include <stdint.h>
#include <emmintrin.h>
#include "Base/PerfDefs.h"

namespace DecoderUtf8 {

struct CoreInfo {
	__m128i shufAB;						//shuffling mask to get lower two bytes of symbols
	union {
		__m128i shufC;					//shuffling mask to get third bytes of symbols
		struct {
			uint32_t _shufC_part0;
			uint32_t _shufC_part1;
			uint32_t srcStep;			//number of bytes processed in input buffer
			uint32_t dstStep;			//number of symbols produced in output buffer (doubled)
		};
	};
};
struct ValidationInfo {
    __m128i headerMask;		//mask of "111..10" bits required in each byte
    __m128i maxValues;		//maximal allowed values of resulting symbols (signed 16-bit)
};

//a single entry of each LUT is defined
struct LutEntryCore : CoreInfo {};
struct LutEntryValidate : LutEntryCore, ValidationInfo {};

//LUT tables are stored as global arrays
extern CACHEALIGN LutEntryCore lutTableCore[32768];
extern CACHEALIGN LutEntryValidate lutTableValidate[32768];

//convenient templated access to LUT tables
#define LUT_TABLE(validate) \
	(validate ? lutTableValidate : lutTableCore)
#define LUT_STRIDE(validate) \
	((validate ? sizeof(LutEntryValidate) : sizeof(LutEntryCore)) / 2)
#define LUT_ACCESS(ptr, index, stride) \
	(const LutEntryValidate *)( \
		(const char *)(ptr) + (stride) * (index) \
	)

}
