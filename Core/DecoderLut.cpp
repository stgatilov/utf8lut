#include "Core/DecoderLut.h"
#include <assert.h>
#include <string.h>

static inline void SetEntry(DecoderLutEntry<false> &entry,
	__m128i shufAB, __m128i shufC, uint32_t srcStep, uint32_t dstStep,
	__m128i headerMask, __m128i maxValues
) {
	entry.shufAB = shufAB;
	entry.shufC = shufC;
	entry.srcStep = srcStep;
	entry.dstStep = dstStep;
}
static inline void SetEntry(DecoderLutEntry<true> &entry,
	__m128i shufAB, __m128i shufC, uint32_t srcStep, uint32_t dstStep,
	__m128i headerMask, __m128i maxValues
) {
	entry.shufAB = shufAB;
	entry.shufC = shufC;
	entry.srcStep = srcStep;
	entry.dstStep = dstStep;
	entry.headerMask = headerMask;
	entry.maxValues = maxValues;
}


template<bool Validate> void DecoderLutTable<Validate>::ComputeAll() {
	//fill tables with empty values for BAD masks
	DecoderLutEntry<Validate> empty;
	SetEntry(empty,
		_mm_set1_epi8(-1),		//produce zero symbols
		_mm_set1_epi8(-1),		//produce zero symbols
		16,						//skip the whole 16-byte block on error
		0,						//do not move output pointer
		_mm_set1_epi8(-1),		//forbid any bytes except 11111110
		_mm_set1_epi16(-1)		//forbid non-negative symbols values (zeros)
	);
	for (int i = 0; i < 32768; i++)
		data[i] = empty;
	//start recursive search for valid masks
	int sizes[32];
	ComputeRec(sizes, 0, 0);	//about 25K entries total
}

template<bool Validate> void DecoderLutTable<Validate>::ComputeRec(int *sizes, int num, int total) {
	if (total >= 16)
		return ComputeEntry(sizes, num);
	for (int sz = 1; sz <= 3; sz++) {
		sizes[num] = sz;
		ComputeRec(sizes, num + 1, total + sz);
	}
}

template<bool Validate> void DecoderLutTable<Validate>::ComputeEntry(const int *sizes, int num) {
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

	//store info into the lookup table
	assert(mask % 2 == 0);  mask /= 2;	//note: odd masks are removed
	SetEntry(data[mask],
		_mm_loadu_si128((__m128i*)shufAB),
		_mm_loadu_si128((__m128i*)shufC),
		preSum,
		2 * cnt,
		_mm_loadu_si128((__m128i*)headerMask),
		_mm_loadu_si128((__m128i*)maxValues)
	);
}

template<bool Validate> const DecoderLutTable<Validate> *DecoderLutTable<Validate>::CreateInstance() {
	static DecoderLutTable<Validate> *singletonTable = 0;
	if (!singletonTable) {
		singletonTable = (DecoderLutTable<Validate> *)_mm_malloc(sizeof(DecoderLutTable<Validate>), CACHE_LINE);
		singletonTable->ComputeAll();
	}
	return singletonTable;
}


template struct DecoderLutTable<false>;
template struct DecoderLutTable<true>;
