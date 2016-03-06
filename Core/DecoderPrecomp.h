#pragma once

#include <stdint.h>
#include <assert.h>
#include <string.h>
#include "Core/DecoderLut.h"

namespace DecoderUtf8 {

void PrecomputeCreateEntry(const int *sizes, int num) {
	//find maximal number of chars to decode
	int cnt = num - 1;
	int preSum = 0;
	for (int i = 0; i < cnt; i++)
		preSum += sizes[i];
	assert(preSum < 16);
	//Note: generally, we can process a char only if the next byte is within XMM register
	//However, if the last char takes 3 bytes and fits the register tightly, we can take it too
	if (preSum == 13 && preSum + sizes[cnt] == 16)
		preSum += sizes[cnt++];
	//still cannot process more that 8 chars per register
	while (cnt > 8)
		preSum -= sizes[--cnt];

	//generate bitmask
	int mask = 0;
	int pos = 0;
	for (int i = 0; i < num; i++)
		for (int j = 0; j < sizes[i]; j++)
			mask |= (j>0) << (pos++);
	assert(pos >= 16);
	mask &= 0xFFFF;

	//generate shuffle masks
	char shufAB[16], shufC[16];
	memset(shufAB, -1, sizeof(shufAB));
	memset(shufC, -1, sizeof(shufC));
	pos = 0;
	for (int i = 0; i < cnt; i++) {
		int sz = sizes[i];
		for (int j = sz-1; j >= 0; j--) {
			if (j < 2)
				shufAB[2 * i + j] = pos++;
			else
				shufC[i] = pos++;
		}
	}
	assert(pos <= 16);

	//generate header masks for validation
	char headerMask[16];
	memset(headerMask, 0, sizeof(headerMask));
	pos = 0;
	for (int i = 0; i < cnt; i++) {
		int sz = sizes[i];
		for (int j = 0; j < sz; j++) {
			int bits = 2;
			if (j == 0)
				bits = (sz == 1 ? 1 : sz == 2 ? 3 : 4);
			headerMask[pos++] = -char(1 << (8 - bits));
		}
	}
	assert(pos <= 16);

	//generate max symbols values for validation 
	int16_t maxValues[8];
	for (int i = 0; i < 8; i++) {
		int sz = (i < cnt ? sizes[i] : 3);
		maxValues[i] = (sz == 1 ? (1<<7) : sz == 2 ? (1<<11) : (1<<15)) - 1;
	}

	//store info into all the lookup tables
	assert(mask % 2 == 0);  mask /= 2;
	{
		LutEntryCore &entry = lutTableCore[mask];
		entry.shufAB = _mm_loadu_si128((__m128i*)shufAB);
		entry.shufC = _mm_loadu_si128((__m128i*)shufC);
		entry.srcStep = preSum;
		entry.dstStep = 2 * cnt;
	}
	{
		LutEntryValidate &entry = lutTableValidate[mask];
		entry.shufAB = _mm_loadu_si128((__m128i*)shufAB);
		entry.shufC = _mm_loadu_si128((__m128i*)shufC);
		entry.srcStep = preSum;
		entry.dstStep = 2 * cnt;
		entry.headerMask = _mm_loadu_si128((__m128i*)headerMask);
		entry.maxValues = _mm_loadu_si128((__m128i*)maxValues);
	}
}
void PrecomputeLutRec(int *sizes, int num, int total) {
	if (total >= 16)
		return PrecomputeCreateEntry(sizes, num);
	for (int sz = 1; sz <= 3; sz++) {
		sizes[num] = sz;
		PrecomputeLutRec(sizes, num + 1, total + sz);
	}
}
void PrecomputeLookupTable() {
	//fill tables with empty values for BAD masks
	LutEntryValidate empty;
	empty.srcStep = 16;						//skip the whole 16-byte block on error
	empty.dstStep = 0;						//do not move output pointer
	empty.shufAB = _mm_set1_epi8(-1);		//produce zero symbols
	empty.shufC = _mm_set1_epi8(-1);		//produce zero symbols
	empty.headerMask = _mm_set1_epi8(-1);	//forbid any bytes except 11111110
	empty.maxValues = _mm_set1_epi16(-1);	//forbid non-negative symbols values (zeros)
	for (int i = 0; i < 32768; i++) {
		lutTableCore[i] = (const LutEntryCore &)empty;
		lutTableValidate[i] = empty;
	}
	//start recursive search for valid masks
	int sizes[32];
	PrecomputeLutRec(sizes, 0, 0);	//about 25K entries total
}

}
