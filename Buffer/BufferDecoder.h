#pragma once

#include <stdint.h>
#include <assert.h>
#include "Base/PerfDefs.h"
#include "Core/DecoderLut.h"
#include "Core/DecoderProcess.h"
#include "Core/ProcessTrivial.h"
#include "Base/Timing.h"

FORCEINLINE const char *FindUtf8Border(const char *pSource) {
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
 * MaxBytes = 1, 2, 3
 * StreamsNum = 0, 1, 4
 * Mode = fast, full, validate
 * OutputType = 2, 4
 */

enum DecoderMode {
	dmFast,		//decode only byte lengths under limit, no checks
	dmFull,		//decode any UTF-8 chars (with fallback to slow version)
	dmValidate,	//decode any UTF-8 chars, validate input
	dmAllCount,	//helper
};

template<int MaxBytes, int OutputType, int Mode, int StreamsNum/*, int BufferSize*/>
class BufferDecoder : public BaseBufferProcessor {
public:
	static const int StreamsNumber = DMAX(StreamsNum, 1);

private:
	static const bool Validate = (Mode == dmValidate);

/*	char inputBuffer[InputBufferSize];
	char outputBuffer[StreamsNumber][OutputBufferSize];
	int inputSize;							//total number of bytes stored in input buffer
	int inputDone;							//number of (first) bytes processed from the input buffer
	int outputSize[StreamsNumber];			//number of bytes stored in each output buffer*/

	static FORCEINLINE bool ProcessSimple(const char *&inputPtr, const char *inputEnd, char *&outputPtr, bool isLastBlock) {
		bool ok = true;
		const DecoderLutEntry<Validate> *RESTRICT ptrTable = DecoderLutTable<Validate>::GetArray();
		while (inputPtr <= inputEnd - 16) {
			ok = DecoderCore<MaxBytes, Mode != dmFast, Mode == dmValidate, OutputType>()(inputPtr, outputPtr, ptrTable);
			if (!ok) {
				if (Mode != dmFast)
					ok = DecodeTrivial<OutputType>(inputPtr, inputPtr + 16, outputPtr);
				if (!ok) break;
			}
		}
		if (isLastBlock)
			ok = DecodeTrivial<OutputType>(inputPtr, inputEnd, outputPtr);
		return ok;
	}

	static FORCEINLINE void SplitRange(const char *buffer, int size, const char *splits[]) {
		splits[0] = buffer;
		splits[StreamsNumber] = buffer + size;
		for (int k = 1; k < StreamsNumber; k++)
			splits[k] = FindUtf8Border(buffer + uint32_t(k * size) / StreamsNumber);
	}
public:

	BufferDecoder() {
		static_assert(MaxBytes >= 1 && MaxBytes <= 3, "MaxBytes must be between 1 and 3");
		static_assert(OutputType == 2 || OutputType == 4, "OutputType must be either 2 or 4");
		static_assert(Mode >= 0 && Mode <= dmAllCount, "Mode must be from DecoderMode enum");
		static_assert(StreamsNum == 0 || StreamsNum == 1 || StreamsNum == 4, "StreamsNum can be only 0, 1 or 4");
//		static_assert(InputBufferSize / StreamsNumber >= MinBytesPerStream, "BufferSize is too small");
		//Clear();
	}

/*	//start completely new compression
	void Clear() {
		inputSize = 0;
		inputDone = 0;
		for (int i = 0; i < StreamsNumber; i++) outputSize[i] = 0;
	}*/

/*	//switch from the just processed block to the next block
	int GetUnprocessedBytesCount() const { return inputSize - inputDone; }
	void GetOutputBuffer(const char *&outputStart, int &outputLen, int bufferIndex = 0) const {
		outputStart = outputBuffer[bufferIndex];
		outputLen = outputSize[bufferIndex];
	}
	void ToNextBlock() {
		memmove(inputBuffer, inputBuffer + inputDone, inputSize - inputDone);
		inputSize = inputSize - inputDone;
		inputDone = 0;
		for (int i = 0; i < StreamsNumber; i++) outputSize[i] = 0;
	}
	void GetInputBuffer(char *&inputStart, int &inputMaxSize) {
		inputStart = inputBuffer + inputSize;
		inputMaxSize = InputBufferSize - inputSize;
	}
	void AddInputSize(int inputSizeAdded) {
		inputSize += inputSizeAdded;
		assert(inputSize <= InputBufferSize);
	}*/

	virtual int GetStreamsCount() const {
		return StreamsNumber;
	}
	virtual int GetInputBufferRecommendedSize() const {
		return 1<<16;	//64KB
	}
	virtual int GetOutputBufferMinSize(int inputSize) const {
		return (inputSize / StreamsNumber + 4) * OutputType;
	}

	virtual bool _Process() {
		TIMING_START(DECODE);
		static const int MinBytesPerStream = 32;	//more than 16 after split
		if (StreamsNum > 1 && inputSize >= StreamsNum * MinBytesPerStream) {
			assert(StreamsNum == 4);
			const char *splits[StreamsNum + 1];
			SplitRange(inputBuffer, inputSize, splits);
			const DecoderLutEntry<Validate> *RESTRICT ptrTable = DecoderLutTable<Validate>::GetArray();
			#define STREAM_START(k) \
				const char *inputStart##k = splits[k]; \
				const char *inputEnd##k = splits[k+1]; \
				const char *inputPtr##k = inputStart##k; \
				char *outputPtr##k = outputBuffer[k];
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
				outputDone[k] = outputPtr##k - outputBuffer[k]; \
				if (!ok##k) \
					return false; \
				if (k+1 < StreamsNum) assert(inputPtr##k == inputEnd##k);
			STREAM_FINISH(0);
			STREAM_FINISH(1);
			STREAM_FINISH(2);
			STREAM_FINISH(3);
		}
		else {
			const char *inputPtr = inputBuffer;
			char *outputPtr = outputBuffer[0];
			bool ok;
			if (StreamsNum == 1) 
				ok = ProcessSimple(inputPtr, inputBuffer + inputSize, outputPtr, lastBlockMode);
			else
				ok = DecodeTrivial<OutputType>(inputPtr, inputBuffer + inputSize, outputPtr);
        	inputDone = inputPtr - inputBuffer;
			outputDone[0] = outputPtr - outputBuffer[0];
			if (!ok) return false;
		}
		TIMING_END(DECODE, inputDone);
		return true;
	}
};
