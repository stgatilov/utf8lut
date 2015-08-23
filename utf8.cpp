#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <tmmintrin.h>
#include "PerfDefs.h"
#include <intrin.h>
#include <stdlib.h>
#ifdef TIMING
	#include "BOM_Profiler.h"
#endif

#ifdef SSE4
	#include "smmintrin.h"
	#define _mm_cmp_allzero(reg) _mm_test_all_zeros(reg, reg)
	#define _mm_cmp_allone(reg) _mm_test_all_ones(reg)
#else
	#define _mm_cmp_allzero(reg) (_mm_movemask_epi8(reg) == 0)
	#define _mm_cmp_allone(reg) (_mm_movemask_epi8(reg) == 0xFFFF)
#endif

struct CoreInfo {
	uint32_t srcStep;		//number of bytes processed in input buffer
	uint32_t dstStep;		//number of symbols produced in output buffer (doubled)
	__m128i shufAB;			//shuffling mask to get lower two bytes of symbols
	__m128i shufC;			//shuffling mask to get third bytes of symbols
};
struct ValidationInfo {
    __m128i headerMask;		//mask of "111..10" bits required in each byte
    __m128i maxValues;		//maximal allowed values of resulting symbols (signed 16-bit)
};

//a single entry of each LUT is defined
struct LutEntryCore : CoreInfo {};
struct LutEntryValidate : LutEntryCore, ValidationInfo {};

//LUT tables are stored as global arrays (with templated access)
static CACHEALIGN LutEntryCore lutTableCore[32768];
static CACHEALIGN LutEntryValidate lutTableValidate[32768];
#define LUT_REF(index, validate) \
	*(const LutEntryValidate *RESTRICT)( \
		(validate ? (const char *)lutTableValidate : (const char *)lutTableCore) + \
		( (validate ? sizeof(LutEntryValidate) : sizeof(LutEntryCore)) / 2) * (index) \
	)

//========================= Precomputation ==========================

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
				shufC[2 * i] = pos++;
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
		entry.srcStep = preSum;
		entry.dstStep = 2 * cnt;
		entry.shufAB = _mm_loadu_si128((__m128i*)shufAB);
		entry.shufC = _mm_loadu_si128((__m128i*)shufC);
	}
	{
		LutEntryValidate &entry = lutTableValidate[mask];
		entry.srcStep = preSum;
		entry.dstStep = 2 * cnt;
		entry.shufAB = _mm_loadu_si128((__m128i*)shufAB);
		entry.shufC = _mm_loadu_si128((__m128i*)shufC);
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

//========================= Core Utils ==========================

FORCEINLINE const char *FindBorder(const char *pSource) {
	for (int i = 0; i < 4; i++) {
		uint8_t byte = pSource[i];
		if ((byte & 0x80U) == 0x00U)
			return pSource + i;
		if ((byte & 0xC0U) == 0xC0U)
			return pSource + i;
	}
	//input not valid: any border is OK
	return pSource;
}

/** template params:
 * MaxBytes = 1, 2, 3
 * CheckExceed = false, true
 * Validate = false, true
 * OutputType = 2, 4 		//UTF16/32
 */

namespace DfaUtf8 {
	#include "dfa.h"
}

template<int OutputType>
FORCEINLINE bool DecodeTrivial(const char *&pSource, char *&pDest, const char *pEnd) {
	using namespace DfaUtf8;
	assert(pSource <= pEnd);

	const uint8_t *RESTRICT s = (const uint8_t *)pSource;
	uint16_t *RESTRICT d = (uint16_t *)pDest;
	uint32_t codepoint;
	uint32_t state = 0;

	const uint8_t *ans_s = s;
	uint16_t *ans_d = d;

	while (s < (const uint8_t *)pEnd) {
		if (decode(&state, &codepoint, *s++))
			continue;
		if (OutputType == 2) {
			if (codepoint > 0xffff) {
				*d++ = (uint16_t)(0xD7C0 + (codepoint >> 10));
				*d++ = (uint16_t)(0xDC00 + (codepoint & 0x3FF));
			} else {
				*d++ = (uint16_t)codepoint;
			}
		}
		else {
			*(uint32_t *)d = codepoint;
			d += 2;
		}
		if (state == UTF8_ACCEPT) {
			ans_s = s;
			ans_d = d;
		}
	}

	pSource = (const char *)ans_s;
	pDest = (char *)ans_d;
	return state != UTF8_REJECT;
}

template<int MaxBytes, bool CheckExceed, bool Validate, int OutputType>
struct DecoderCore {
	FORCEINLINE bool operator()(const char *&ptrSource, char *&ptrDest) {
		static_assert(!Validate || CheckExceed, "Validate core mode requires CheckExceed enabled");
		const char *RESTRICT pSource = ptrSource;
		char *RESTRICT pDest = ptrDest;

		if (MaxBytes == 1) {
			__m128i reg = _mm_loadu_si128((__m128i*)pSource);
			__m128i zero = _mm_setzero_si128();
			if (CheckExceed && _mm_movemask_epi8(reg))
				return false;
			__m128i half0 = _mm_unpacklo_epi8(reg, zero);
			__m128i half1 = _mm_unpackhi_epi8(reg, zero);
			if (OutputType == 2) {
				_mm_storeu_si128((__m128i*)pDest + 0, half0);
				_mm_storeu_si128((__m128i*)pDest + 1, half1);
			}
			else {
				_mm_storeu_si128((__m128i*)pDest + 0, _mm_unpacklo_epi16(half0, zero));
				_mm_storeu_si128((__m128i*)pDest + 1, _mm_unpackhi_epi16(half0, zero));
				_mm_storeu_si128((__m128i*)pDest + 2, _mm_unpacklo_epi16(half1, zero));
				_mm_storeu_si128((__m128i*)pDest + 3, _mm_unpackhi_epi16(half1, zero));
			}
			ptrSource += 16;
			ptrDest += 16 * OutputType;
			return true;
		}
		else {	//MaxBytes = 2 or 3
			__m128i reg = _mm_loadu_si128((__m128i*)pSource);
			if (CheckExceed && !Validate) {
				__m128i pl = _mm_add_epi8(reg, _mm_set1_epi8(0x80U));
				__m128i cmpRes = _mm_cmpgt_epi8(pl, _mm_set1_epi8(MaxBytes == 3 ? 0x6F : 0x5F));
				if (!_mm_cmp_allzero(cmpRes))
					return false;
			}

			uint32_t mask = _mm_movemask_epi8(_mm_cmplt_epi8(reg, _mm_set1_epi8(0xC0U)));
			if (Validate && (mask & 1))
				return false;
			const LutEntryValidate &lookup = LUT_REF(mask, Validate);

			__m128i Rab = _mm_shuffle_epi8(reg, lookup.shufAB);
			Rab = _mm_and_si128(Rab, _mm_setr_epi8(
				0x7F, 0x3F, 0x7F, 0x3F, 0x7F, 0x3F, 0x7F, 0x3F, 0x7F, 0x3F, 0x7F, 0x3F, 0x7F, 0x3F, 0x7F, 0x3F
			));
			Rab = _mm_maddubs_epi16(Rab, _mm_setr_epi8(
				0x01, 0x40, 0x01, 0x40, 0x01, 0x40, 0x01, 0x40, 0x01, 0x40, 0x01, 0x40, 0x01, 0x40, 0x01, 0x40
			));
			__m128i sum = Rab;

			if (MaxBytes == 3) {
				__m128i Rc = _mm_shuffle_epi8(reg, lookup.shufC);
				Rc = _mm_slli_epi16(Rc, 12);
				sum = _mm_add_epi16(sum, Rc);
			}

			if (Validate) {
				__m128i byteMask = lookup.headerMask;
				__m128i header = _mm_and_si128(reg, byteMask);
				__m128i hdrRef = _mm_add_epi8(byteMask, byteMask);
				__m128i hdrCorrect = _mm_cmpeq_epi8(header, hdrRef);
				__m128i overlongSymbol = _mm_cmpgt_epi16(sum, lookup.maxValues);
				if (MaxBytes == 2)
					hdrCorrect = _mm_and_si128(hdrCorrect, lookup.shufC);	//forbid 3-byte symbols
				__m128i allCorr = _mm_andnot_si128(overlongSymbol, hdrCorrect);	
				if (!_mm_cmp_allone(allCorr))
					return false;
			}

			if (OutputType == 2)
				_mm_storeu_si128((__m128i*)pDest, sum);
			else {
				__m128i zero = _mm_setzero_si128();
				_mm_storeu_si128((__m128i*)pDest + 0, _mm_unpacklo_epi16(sum, zero));
				_mm_storeu_si128((__m128i*)pDest + 1, _mm_unpackhi_epi16(sum, zero));
			}
			ptrSource += lookup.srcStep;
			ptrDest += lookup.dstStep * (OutputType/2);

			return true;
		}
	}
};

//========================= Buffer operations ==========================
/**params:
 * maxBytes = 1, 2, 3
 * numStreams = 1, 4
 * validate = false, true
 * mode = fast, full, validate
 */

enum DecoderMode {
	dmFast,		//decode only byte lengths under limit, no checks
	dmFull,		//decode any UTF-8 chars (with fallback to slow version)
	dmValidate,	//decode any UTF-8 chars, validate input
	dmAllCount,	//helper
};

template<int MaxBytes, int OutputType, int Mode, int StreamsNum, int BufferSize>
class BufferDecoder {
	static const int InputBufferSize = BufferSize;
	static const int OutputBufferSize = (BufferSize / StreamsNum + 4) * OutputType;
	static const int MinBytesPerStream = 32;

	char inputBuffer[InputBufferSize];
	char outputBuffer[StreamsNum][OutputBufferSize];
	int inputSize;							//total number of bytes stored in input buffer
	int inputDone;							//number of (first) bytes processed from the input buffer
	int outputSize[StreamsNum];				//number of bytes stored in each output buffer
#ifdef TIMING
	BOM_Table *timings;						//only for timings
#endif

	static FORCEINLINE bool ProcessSimple(const char *&inputPtr, const char *inputEnd, char *&outputPtr, bool isLastBlock) {
		bool ok = true;
		while (inputPtr <= inputEnd - 16) {
			ok = DecoderCore<MaxBytes, Mode != dmFast, Mode == dmValidate, OutputType>()(inputPtr, outputPtr);
			if (!ok) {
				if (Mode != dmFast)
					ok = DecodeTrivial<OutputType>(inputPtr, outputPtr, inputPtr + 16);
				if (!ok) break;
			}
		}
		if (isLastBlock)
			ok = DecodeTrivial<OutputType>(inputPtr, outputPtr, inputEnd);
		return ok;
	}

	static FORCEINLINE void SplitRange(const char *buffer, int size, const char *splits[]) {
		splits[0] = buffer;
		splits[StreamsNum] = buffer + size;
		for (int k = 1; k < StreamsNum; k++)
			splits[k] = FindBorder(buffer + uint32_t(k * size) / StreamsNum);
	}

public:
	static const int StreamsNumber = StreamsNum;

	BufferDecoder() {
		static_assert(MaxBytes >= 1 && MaxBytes <= 3, "MaxBytes must be between 1 and 3");
		static_assert(OutputType == 2 || OutputType == 4, "OutputType must be either 2 or 4");
		static_assert(Mode >= 0 && Mode <= dmAllCount, "Mode must be from DecoderMode enum");
		static_assert(StreamsNum == 1 || StreamsNum == 4, "StreamsNum can be only 1 or 4");
		static_assert(InputBufferSize / StreamsNum >= MinBytesPerStream, "BufferSize is too small");
		Clear();
#ifdef TIMING
		timings = init_BOM_timer();
#endif
	}
	~BufferDecoder() {
#ifdef TIMING
		dump_BOM_table(timings);
		free(timings);
#endif
	}

	//start completely new compression
	void Clear() {
		inputSize = 0;
		inputDone = 0;
		for (int i = 0; i < StreamsNum; i++) outputSize[i] = 0;
	}

	//switch from the just processed block to the next block
	int GetUnprocessedBytesCount() const { return inputSize - inputDone; }
	void GetOutputBuffer(const char *&outputStart, int &outputLen, int bufferIndex = 0) const {
		outputStart = outputBuffer[bufferIndex];
		outputLen = outputSize[bufferIndex];
	}
	void ToNextBlock() {
		memmove(inputBuffer, inputBuffer + inputDone, inputSize - inputDone);
		inputSize = inputSize - inputDone;
		inputDone = 0;
		for (int i = 0; i < StreamsNum; i++) outputSize[i] = 0;
	}
	void GetInputBuffer(char *&inputStart, int &inputMaxSize) {
		inputStart = inputBuffer + inputSize;
		inputMaxSize = BufferSize - inputSize;
	}
	void AddInputSize(int inputSizeAdded) {
		inputSize += inputSizeAdded;
		assert(inputSize <= InputBufferSize);
	}


	//processing currently loaded block
	bool Process(bool isLastBlock = true) {
#ifdef TIMING
		start_BOM_interval(timings);
#endif
		if (StreamsNum > 1 && inputSize >= StreamsNum * MinBytesPerStream) {
			assert(StreamsNum == 4);
			const char *splits[StreamsNum + 1];
			SplitRange(inputBuffer, inputSize, splits);
			#define STREAM_START(k) \
				const char *inputStart##k = splits[k]; \
				const char *inputEnd##k = splits[k+1]; \
				const char *RESTRICT inputPtr##k = inputStart##k; \
				char *RESTRICT outputPtr##k = outputBuffer[k];
			STREAM_START(0)
			STREAM_START(1)
			STREAM_START(2)
			STREAM_START(3)
			while (1) {
				#define STREAM_CHECK(k) \
					if (inputPtr##k > inputEnd##k - 16) \
						break;
				STREAM_CHECK(0);
				STREAM_CHECK(1);
				STREAM_CHECK(2);
				STREAM_CHECK(3);
				#define STREAM_STEP(k) \
					bool ok##k = DecoderCore<MaxBytes, Mode != dmFast, Mode == dmValidate, OutputType>()(inputPtr##k, outputPtr##k); \
					if (!ok##k) break;
				STREAM_STEP(0);
				STREAM_STEP(1);
				STREAM_STEP(2);
				STREAM_STEP(3);
			}
			#define STREAM_FINISH(k) \
				bool ok##k = ProcessSimple(inputPtr##k, inputEnd##k, outputPtr##k, true); \
				inputDone = inputPtr##k - inputBuffer; \
				outputSize[k] = outputPtr##k - outputBuffer[k]; \
				if (!ok##k) \
					return false; \
				if (k+1 < StreamsNum) assert(inputPtr##k == inputEnd##k);
			STREAM_FINISH(0);
			STREAM_FINISH(1);
			STREAM_FINISH(2);
			STREAM_FINISH(3);
		}
		else {
			const char *RESTRICT inputPtr = inputBuffer;
			char *RESTRICT outputPtr = outputBuffer[0];
			bool ok = ProcessSimple(inputPtr, inputBuffer + inputSize, outputPtr, isLastBlock);
        	inputDone = inputPtr - inputBuffer;
			outputSize[0] = outputPtr - outputBuffer[0];
			if (!ok) return false;
		}
#ifdef TIMING
		end_BOM_interval(timings, inputDone);
#endif
		return true;
	}
};

//========================= Global operations ==========================

const uint16_t BOM_UTF16 = 0xFEFFU;
BufferDecoder<3, 2, dmValidate, 4, 1<<16> decoder;

int main() {
	PrecomputeLookupTable();

	FILE *fi = fopen("input.txt", "rb");
	FILE *fo = fopen("output.txt", "wb");
	decoder.Clear();
	while (1) {
		char *inputBuffer;
		int maxSize;
		decoder.GetInputBuffer(inputBuffer, maxSize);
		int readSize = fread(inputBuffer, 1, maxSize, fi);
		decoder.AddInputSize(readSize);
		bool ok = decoder.Process(readSize != maxSize);
		if (!ok) {
			printf("Error in decoding!\n");
			break;
		}
		for (int k = 0; k < decoder.StreamsNumber; k++) {
			const char *outputBuffer;
			int outSize;
			decoder.GetOutputBuffer(outputBuffer, outSize, k);
			fwrite(outputBuffer, outSize, 1, fo);
		}
		decoder.ToNextBlock();
		if (readSize != maxSize)
			break;
	}
	assert(decoder.GetUnprocessedBytesCount() == 0);
	fclose(fi);
	fclose(fo);

	return 0;
}
