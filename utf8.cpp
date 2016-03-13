#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <tmmintrin.h>
#include "Base/PerfDefs.h"
#include <intrin.h>
#include <stdlib.h>
#include "Buffer/BufferDecoder.h"
#include "Buffer/BufferEncoder.h"

const uint16_t BOM_UTF16 = 0xFEFFU;
BufferDecoder<3, 2, dmValidate, 1, 1<<16> decoder;
BufferEncoder<2, 2, dmValidate, 1, 1<<16> encoder;

template<class BufferProcessor> void ProcessFiles(BufferProcessor &processor, FILE *fi, FILE *fo) {
	processor.Clear();
	while (1) {
		char *inputBuffer;
		int maxSize;
		processor.GetInputBuffer(inputBuffer, maxSize);
		int readSize = fread(inputBuffer, 1, maxSize, fi);
		processor.AddInputSize(readSize);
		bool ok = processor.Process(readSize != maxSize);
		if (!ok)
			throw "Input data is invalid!";
		for (int k = 0; k < processor.StreamsNumber; k++) {
			const char *outputBuffer;
			int outSize;
			processor.GetOutputBuffer(outputBuffer, outSize, k);
			fwrite(outputBuffer, outSize, 1, fo);
		}
		processor.ToNextBlock();
		if (readSize != maxSize)
			break;
	}
	if (processor.GetUnprocessedBytesCount() != 0)
		throw "Input data is incomplete!";
}

template<class BufferProcessor> void ProcessFilesByName(BufferProcessor &processor, const char *nameI, const char *nameO) {
	FILE *fi = fopen(nameI, "rb");
	FILE *fo = fopen(nameO, "wb");
	ProcessFiles(processor, fi, fo);
	fclose(fi);
	fclose(fo);
}

int main() {
	//precompute in advance
	DecoderLutTable<false>::CreateInstance();
	DecoderLutTable<true>::CreateInstance();
try {

	//decode file (multiple times for profiling)
	for (int run = 0; run < 100; run++)
//		ProcessFilesByName(decoder, "utf8.txt", "utfXX.txt");

	//encode file (multiple times for profiling)
//	for (int run = 0; run < 100; run++)
		ProcessFilesByName(encoder, "utfXX.txt", "utf8.txt");

	//print profiling info
	TIMING_PRINT();
} catch(const char *str) {
	fprintf(stderr, "Error: %s\n", str);
}

	return 0;
}
