#include "Core/EncoderLut.h"
#include <assert.h>
#include <string.h>

template<bool ThreeBytes> void EncoderLutTable<ThreeBytes>::ComputeAll() {
	//start search for all masks
	for (int lensMask = 0; lensMask < 256; lensMask++)
		ComputeEntry(lensMask);
}

template<> void EncoderLutTable<false>::ComputeEntry(int lensMask) {
	//init shuffle bitmask and header mask
	char shuf[16], header[16];
	memset(shuf, -1, sizeof(shuf));
	memset(header, 0, sizeof(header));
	//go over all 8 input symbols
	int pos = 0;
	for (int i = 0; i < 8; i++) {
		if (lensMask & (1<<i)) {	//two bytes
			shuf[pos] = 2 * i;		//take B[i]
			header[pos] = char(0xE0);
			shuf[pos] = 2 * i + 1;	//take A[i]
			header[pos] = char(0xC0);
			pos += 2;
		}
		else {	//one byte
			shuf[pos] = 2 * i + 1;	//take A[i]
			header[pos] = char(0x80);
			pos++;
		}
	}
	//save data into LUT entry
	EncoderLutEntry &entry = data[lensMask];
	entry.shuf = _mm_loadu_si128((__m128i*)shuf);
	entry.headerMask = _mm_loadu_si128((__m128i*)header);
	entry.dstStep = pos;
}

/*
template<bool ThreeBytes> void EncoderLutTable<ThreeBytes>::ComputeEntry(const int *sizes, int num) {
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
*/

template<bool ThreeBytes> const EncoderLutTable<ThreeBytes> *EncoderLutTable<ThreeBytes>::CreateInstance() {
	static EncoderLutTable<ThreeBytes> *singletonTable = 0;
	if (!singletonTable) {
		singletonTable = (EncoderLutTable<ThreeBytes> *)_mm_malloc(sizeof(EncoderLutTable<ThreeBytes>), CACHE_LINE);
		singletonTable->ComputeAll();
	}
	return singletonTable;
}


template struct EncoderLutTable<false>;
//template struct EncoderLutTable<true>;
