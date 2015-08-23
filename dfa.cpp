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

#include "dfa.h"

template<bool Validate, int OutputType>
FORCEINLINE bool DecodeTrivial(const char *&pSource, char *&pDest, const char *pEnd) {
	assert(pSource <= pEnd);
	const uint8_t *RESTRICT s = (const uint8_t *)pSource;
	uint16_t *RESTRICT d = (uint16_t *)pDest;
	uint32_t codepoint;
	uint32_t state = 0;

	const uint8_t *pEndPadded = (const uint8_t *)(pEnd - 6);
	while (s < pEndPadded) {
		if (decode(&state, &codepoint, *s++))
			continue;
		if (codepoint > 0xffff) {
			*d++ = (uint16_t)(0xD7C0 + (codepoint >> 10));
			*d++ = (uint16_t)(0xDC00 + (codepoint & 0x3FF));
		} else {
			*d++ = (uint16_t)codepoint;
		}
	}

	bool ok = false;
	while (s < (const uint8_t *)pEnd) {
		if (decode(&state, &codepoint, *s++))
			continue;
		if (codepoint > 0xffff) {
			*d++ = (uint16_t)(0xD7C0 + (codepoint >> 10));
			*d++ = (uint16_t)(0xDC00 + (codepoint & 0x3FF));
		} else {
			*d++ = (uint16_t)codepoint;
		}
		if (state == UTF8_ACCEPT) {
			pSource = (const char*)s;
			pDest = (char*)d;
			ok = true;
		}
	}

	return ok;
}

//========================= Buffer operations ==========================

enum DecoderMode {
	dmFast,		//decode only byte lengths under limit, no checks
	dmFull,		//decode any UTF-8 chars (with fallback to slow version)
	dmValidate,	//decode any UTF-8 chars, validate input
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

/*	static FORCEINLINE void SplitRange(const char *buffer, int size, const char *splits[]) {
		splits[0] = buffer;
		splits[StreamsNum] = buffer + size;
		for (int k = 1; k < StreamsNum; k++)
			splits[k] = FindBorder(buffer + uint32_t(k * size) / StreamsNum);
	}*/

public:
	static const int StreamsNumber = StreamsNum;

	BufferDecoder() {
		static_assert(InputBufferSize / StreamsNum >= MinBytesPerStream, "Buffer too small");
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
	
		static_assert(StreamsNum == 1, "Only single stream is supported");
		const char *pSrc = inputBuffer;
		char *pDest = outputBuffer[0];
		bool ok = DecodeTrivial<Mode == dmValidate, OutputType>(pSrc, pDest, inputBuffer + inputSize);
		if (!ok) return false;
		inputDone = pSrc - inputBuffer;
		outputSize[0] = pDest - outputBuffer[0];

#ifdef TIMING
		end_BOM_interval(timings, inputDone);
#endif
		return true;
	}
};

//========================= Global operations ==========================

const uint16_t BOM_UTF16 = 0xFEFFU;
BufferDecoder<3, 2, dmFull, 1, 1<<16> decoder;

int main() {
//	PrecomputeLookupTable();

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
