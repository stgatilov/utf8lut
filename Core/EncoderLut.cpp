#include "Core/EncoderLut.h"
#include <assert.h>
#include <string.h>

template<bool ThreeBytes> void EncoderLutTable<ThreeBytes>::ComputeAll() {
	//start search for all masks
	for (int lensMask = 0; lensMask < 256; lensMask++)
		ComputeEntry(lensMask);
}

typedef int (*PartPosGetter)(int, int);
static void HandleChar(int idx, int len, int &pos, char *shuf, char *header, PartPosGetter getPart) {
	for (int j = 0; j < len; j++) {
		shuf[pos + j] = getPart(idx, len-1 - j);
		header[pos + j] = char(0xC0);
	}
	static const int firstByteHeader[] = {0xFF, 0x80, 0xE0, 0xF0};
	header[pos] = char(firstByteHeader[len]);
	pos += len;
}

int TwoBytesPartPosGetter(int idx, int part) {
	return 2 * idx + (1 - part);
}
template<> void EncoderLutTable<false>::ComputeEntry(int lensMask) {
	//init shuffle bitmask and header mask
	char shuf[16], header[16];
	memset(shuf, -1, sizeof(shuf));
	memset(header, 0, sizeof(header));
	//go over all 8 input symbols
	int pos = 0;
	for (int i = 0; i < 8; i++) {
		int len = 1 + (1 & (lensMask >> i));
		HandleChar(i, len, pos, shuf, header, TwoBytesPartPosGetter);
	}
	//save data into LUT entry
	EncoderLutEntry &entry = data[lensMask];
	entry.shuf = _mm_loadu_si128((__m128i*)shuf);
	entry.headerMask = _mm_loadu_si128((__m128i*)header);
	entry.dstStep = pos;
}

int ThreeBytesPartPosGetter(int idx, int part) {
	if (part < 2)
		return 2 * idx + part;
	else
		return 8 + 2 * idx;
}
template<> void EncoderLutTable<true>::ComputeEntry(int lensMask) {
	//init shuffle bitmask and header mask
	char shuf[16], header[16];
	memset(shuf, -1, sizeof(shuf));
	memset(header, 0, sizeof(header));
	//go over all 8 input symbols
	int pos = 0;
	int index = 0;
	for (int i = 0; i < 4; i++) {
		int len = 1 + (3 & (lensMask >> (2 * i)));
		if (len > 3)
			return;	//impossible entry
		HandleChar(i, len, pos, shuf, header, ThreeBytesPartPosGetter);
		if (len >= 2)
			index ^= (1 << i);
		if (len >= 3)
			index ^= (1 << (4 + i));
	}
	//save data into LUT entry
	EncoderLutEntry &entry = data[index];
	entry.shuf = _mm_loadu_si128((__m128i*)shuf);
	entry.headerMask = _mm_loadu_si128((__m128i*)header);
	entry.dstStep = pos;
}

template<bool ThreeBytes> const EncoderLutTable<ThreeBytes> *EncoderLutTable<ThreeBytes>::CreateInstance() {
	static EncoderLutTable<ThreeBytes> *singletonTable = 0;
	if (!singletonTable) {
		singletonTable = (EncoderLutTable<ThreeBytes> *)_mm_malloc(sizeof(EncoderLutTable<ThreeBytes>), CACHE_LINE);
		singletonTable->ComputeAll();
	}
	return singletonTable;
}


template struct EncoderLutTable<false>;
template struct EncoderLutTable<true>;
