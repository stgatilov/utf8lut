#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>


#ifdef _MSC_VER
    #include <intrin.h>
#else
    #include <x86intrin.h>
#endif
static inline uint64_t get_ticks() {
    return __rdtsc();
}

void ReadFileContents(const char *filename, char *&buffer, long long &size) {
    assert(!buffer);
    FILE *fi = fopen(filename, "rb");
    fseek(fi, 0, SEEK_END);
    size = ftell(fi);
    fseek(fi, 0, SEEK_SET);
    buffer = new char[size];
    fread(buffer, 1, size, fi);
    fclose(fi);
}

void WriteFileContents(const char *filename, char *buffer, long long size) {
    FILE *fo = fopen(filename, "wb");
    fwrite(buffer, 1, size, fo);
    fclose(fo);
}


extern size_t fromUtf8(const char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft);

int main(int argc, char **argv) {
    const char *infn = argv[1];
    const char *outfn = argv[2];
    int numberOfRuns = atoi(argv[3]);
    
    clock_t startTime = clock();
    uint64_t startTicks = get_ticks();

    char *inputData = 0;
    long long inputSize = 0;
    ReadFileContents(infn, inputData, inputSize);
    long long outputSize = inputSize * 2;
    char *outputData = new char[outputSize];
    printf("Read input data (%d bytes)\n", int(inputSize));

    long long outputActualSize = 0;
    for (int r = 0; r < numberOfRuns; r++) {
        const char *inPtr = inputData;
        char *outPtr = outputData;
        size_t inBytes = inputSize;
        size_t outBytes = outputSize;
        fromUtf8(&inPtr, &inBytes, &outPtr, &outBytes);
        outputActualSize = outPtr - outputData;
    }

    WriteFileContents(outfn, outputData, outputActualSize);

    clock_t endTime = clock();
    uint64_t endTicks = get_ticks();
    double elapsedTime = double(endTime - startTime) / CLOCKS_PER_SEC;

    printf("The task was finished in %0.3f seconds\n", elapsedTime);
    double ticksPerElem = double(endTicks - startTicks) / numberOfRuns / inputSize;
    printf("From total time   :  %0.3lf cyc/el\n", ticksPerElem);

    return 0;
}
