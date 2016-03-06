#pragma once

#include <stdint.h>
#include <assert.h>
#include "Base/PerfDefs.h"
#include "Core/DecoderLut.h"
#include "Core/DecoderProcess.h"
#include "Core/DfaProcess.h"

#ifdef TIMING
	#include "Base/BOM_Profiler.h"
#endif

namespace DecoderUtf8 {

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
		const LutEntryCore *RESTRICT ptrTable = LUT_TABLE(Mode == dmValidate);
		while (inputPtr <= inputEnd - 16) {
			ok = DecoderCore<MaxBytes, Mode != dmFast, Mode == dmValidate, OutputType>()(inputPtr, outputPtr, ptrTable);
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
			const LutEntryCore *RESTRICT ptrTable = LUT_TABLE(Mode == dmValidate);
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
					bool ok##k = DecoderCore<MaxBytes, Mode != dmFast, Mode == dmValidate, OutputType>()(inputPtr##k, outputPtr##k, ptrTable); \
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

}
