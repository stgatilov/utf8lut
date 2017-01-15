#pragma once

#include "Buffer/BaseBufferProcessor.h"
#include "Buffer/ProcessorPlugins.h"

enum ConversionStatus {
    csSuccess = 0,
    //Conversion performed successfully.
    //Input data is correct and complete.
    //Output was properly written.

    csIncompleteData = 1,
    //Input data is correct but INCOMPLETE.
    //Converted and written to output maximal number of valid characters from input.

    csOverflowPossible = 2,
    //Conversion not attempted.
    //Output buffer is not large enough to hold any potential converted output.

    csIncorrectData = 3,
    //Input data is INCORRECT.
    //Conversion is done partially, but cannot be finished due to invalid input.
    //Output buffer contains converted data for some part of the input (generally can be ignored).
};

struct ConversionResult {
    //overall result of conversion (see above)
    ConversionStatus status;
    //number of bytes read from input
    long long inputSize;
    //number of bytes written to output
    long long outputSize;
};


//======================================= In-memory conversion ====================================

struct ConvertInMemorySettings {
    //if true, then output is written using non-temporal writes
    //bool writeNonTemporal;

    ConvertInMemorySettings() /*: writeNonTemporal(true)*/ {}
};

//Convert from contiguous buffer in memory into another (non-overlapping) contiguous buffer in memory.
ConversionResult ConvertInMemory(BaseBufferProcessor &processor, const char *inputBuffer, long long inputSize, char *outputBuffer, long long outputSize, ConvertInMemorySettings settings = ConvertInMemorySettings());

//Request information about size of output buffer in ConvertInMemory routine.
//Returns minimal allowed size of output buffer in bytes.
//If outputBuffer points to char* with null value, then:
//   1. a buffer of that size will be allocated (with new char[]);
//   2. pointer to this new buffer will be saved into the pointer;
long long ConvertInMemorySize(BaseBufferProcessor &processor, long long inputSize, char **outputBuffer = 0);


//======================================= On-disk conversion ======================================

enum FileIOType {
    ftLibC = 0,
    //Use fopen + fread + fwrite + fclose.
    //Cross-platform.

    /*ftPosix = 1,
    //open + read + write + close
    //Note: requires Posix OS  (Windows also implements this API).*/

    //Perhaps we can add some more types...
    //ftWinApi,
    //ftPosixAsync,
    //ftWinApiAsync,
    //...
};

struct ConvertFilesSettings {
    //which functions to use for IO
    FileIOType type;
    /*//hint about buffer size used for I/O requests
    int bufferSize;*/

    ConvertFilesSettings() : type(ftLibC)/*, bufferSize(0)*/ {}
};

ConversionResult ConvertFiles(BaseBufferProcessor &processor, const char *inputFilePath, const char *outputFilePath, ConvertFilesSettings settings = ConvertFilesSettings());
