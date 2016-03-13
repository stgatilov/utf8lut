#pragma once

#include <stdint.h>
#include <assert.h>
#include "Base/PerfDefs.h"
#include "Core/EncoderLut.h"
#include "Core/EncoderProcess.h"
#include "Core/ProcessTrivial.h"
#include "Base/Timing.h"

/**params:
 * MaxBytes = 1, 2, 3
 * UnrollNum = 0, 1, 4
 * Mode = fast, full, validate
 * InputType = 2, 4
 */

enum EncoderMode {
	emFast,		//encode only byte lengths under limit, no checks
	emFull,		//encode any UTF-8 chars (with fallback to slow version)
	emValidate,	//encode any UTF-8 chars, validate input
	emAllCount,	//helper
};

template<int MaxBytes, int InputType, int Mode, int UnrollNum, int BufferSize>
class BufferEncoder {
public:
	static const int StreamsNumber = 1;

private:
	static const int InputBufferSize = BufferSize;
	static const int OutputBufferSize = (BufferSize / InputType) * 3 + 16;
	static const int InputWordsQuant = 64;
	static const bool ThreeBytes = (MaxBytes >= 3);

	char inputBuffer[InputBufferSize];
	char outputBuffer[OutputBufferSize];
	int inputSize;							//total number of bytes stored in input buffer
	int inputDone;							//number of (first) bytes processed from the input buffer
	int outputSize;							//number of bytes stored in the output buffer

	static FORCEINLINE bool ProcessSimple(const char *&inputPtr, const char *inputEnd, char *&outputPtr, bool isLastBlock) {
		bool ok = true;
		const EncoderLutEntry *RESTRICT ptrTable = EncoderLutTable<ThreeBytes>::GetArray();
		while (inputPtr <= inputEnd - 16) {
			ok = EncoderCore<MaxBytes, Mode != dmFast, Mode == dmValidate, InputType>()(inputPtr, outputPtr, ptrTable);
			if (!ok) {
				if (Mode != dmFast)
					ok = EncodeTrivial<InputType>(inputPtr, inputPtr + 16, outputPtr);
				if (!ok) break;
			}
		}
		if (isLastBlock)
			ok = EncodeTrivial<InputType>(inputPtr, inputEnd, outputPtr);
		return ok;
	}

public:

	BufferEncoder() {
		static_assert(MaxBytes >= 1 && MaxBytes <= 3, "MaxBytes must be between 1 and 3");
		static_assert(InputType == 2 || InputType == 4, "InputType must be either 2 or 4");
		static_assert(Mode >= 0 && Mode <= dmAllCount, "Mode must be from EncoderMode enum");
		static_assert(UnrollNum == 0 || UnrollNum == 1 || UnrollNum == 4, "UnrollNum must be 0, 1 or 4");
		static_assert(InputBufferSize > 0 && InputBufferSize % InputWordsQuant == 0, "BufferSize is either small or not aligned enough");
		Clear();
	}

	//start completely new compression
	void Clear() {
		inputSize = 0;
		inputDone = 0;
		outputSize = 0;
	}

	//switch from the just processed block to the next block
	int GetUnprocessedBytesCount() const { return inputSize - inputDone; }
	void GetOutputBuffer(const char *&outputStart, int &outputLen, int) const {
		outputStart = outputBuffer;
		outputLen = outputSize;
	}
	void ToNextBlock() {
		memmove(inputBuffer, inputBuffer + inputDone, inputSize - inputDone);
		inputSize = inputSize - inputDone;
		inputDone = 0;
		outputSize = 0;
	}
	void GetInputBuffer(char *&inputStart, int &inputMaxSize) {
		inputStart = inputBuffer + inputSize;
		inputMaxSize = InputBufferSize - inputSize;
	}
	void AddInputSize(int inputSizeAdded) {
		inputSize += inputSizeAdded;
		assert(inputSize <= InputBufferSize);
	}


	//processing currently loaded block
	bool Process(bool isLastBlock = true) {
		TIMING_START(ENCODE);

		const char *RESTRICT inputPtr = inputBuffer;
		char *RESTRICT outputPtr = outputBuffer;
		bool ok;
		if (UnrollNum == 1)
			ok = ProcessSimple(inputPtr, inputBuffer + inputSize, outputPtr, isLastBlock);
		else
			ok = EncodeTrivial<InputType>(inputPtr, inputBuffer + inputSize, outputPtr);
       	inputDone = inputPtr - inputBuffer;
		outputSize = outputPtr - outputBuffer;
		if (!ok) return false;

		TIMING_END(ENCODE, inputDone);
		return true;
	}
};
