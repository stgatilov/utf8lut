#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <tmmintrin.h>
#include "Base/PerfDefs.h"
#include <intrin.h>
#include <stdlib.h>
#include "Buffer/BufferDecoder.h"

const uint16_t BOM_UTF16 = 0xFEFFU;
BufferDecoder<3, 2, dmValidate, 1, 1<<16> decoder;

void DecodeFiles(FILE *fi, FILE *fo) {
	decoder.Clear();
	while (1) {
		char *inputBuffer;
		int maxSize;
		decoder.GetInputBuffer(inputBuffer, maxSize);
		int readSize = fread(inputBuffer, 1, maxSize, fi);
		decoder.AddInputSize(readSize);
		bool ok = decoder.Process(readSize != maxSize);
		if (!ok)
			throw "Input data is not UTF-8!\n";
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
}

void DecodeFiles(const char *nameI, const char *nameO) {
	FILE *fi = fopen(nameI, "rb");
	FILE *fo = fopen(nameO, "wb");
	DecodeFiles(fi, fo);
	fclose(fi);
	fclose(fo);
}

int main() {
	//precompute in advance
	DecoderLutTable<false>::CreateInstance();
	DecoderLutTable<true>::CreateInstance();

	//decode file (multiple times for profiling)
	for (int run = 0; run < 100; run++)
		DecodeFiles("input.txt", "output.txt");

	//print profiling info
	TIMING_PRINT();

	return 0;
}
