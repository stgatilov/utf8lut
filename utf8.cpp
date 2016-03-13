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

int main() {

	//precompute in advance
	DecoderLutTable<false>::CreateInstance();
	DecoderLutTable<true>::CreateInstance();

for (int run = 0; run < 100; run++) {

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
}

	TIMING_PRINT();
	return 0;
}
