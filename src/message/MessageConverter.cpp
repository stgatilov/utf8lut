#include "Message/MessageConverter.h"
#include <assert.h>

ConversionResult ConvertInMemory(BaseBufferProcessor &processor, const char *inputBuffer, long long inputSize, char *outputBuffer, long long outputSize, ConvertInMemorySettings settings) {
    assert(inputBuffer || inputSize == 0);
    assert(outputBuffer || outputSize == 0);

    ConversionResult result;
    result.status = (ConversionStatus)-1;
    result.inputSize = 0;
    result.outputSize = 0;

    long long reqSize = ContiguousOutput::GetMaxOutputSize(processor, inputSize);
    if (outputSize < reqSize) {
        result.status = csOverflowPossible;
        return result;
    }

    processor.Clear();
    ContiguousInput input(processor, inputBuffer, inputSize);
    ContiguousOutput output(processor, outputBuffer, outputSize);

    while (!input.Finished()) {
        //do all the work
        bool ok = processor.Process();

        //check if hard error occurred
        if (!ok) {
            result.status = csIncorrectData;
            //TODO: do we always properly set the sizes/bytes for 4 streams?
            result.inputSize = input.GetProcessedInputSize();
            result.outputSize = output.GetFilledOutputSize();
            return result;
        }
    }

    //check if some bytes in the input remain
    if (input.GetRemainingDataSize() != 0) {
        result.status = csIncorrectData;
        result.inputSize = input.GetProcessedInputSize();
        result.outputSize = output.GetFilledOutputSize();
        return result;
    }

    result.status = csSuccess;
    result.inputSize = input.GetProcessedInputSize();
    result.outputSize = output.GetFilledOutputSize();
    return result;
}

long long ConvertInMemorySize(BaseBufferProcessor &processor, long long inputSize, char **outputBuffer) {
    long long reqSize = ContiguousOutput::GetMaxOutputSize(processor, inputSize);
    if (outputBuffer && *outputBuffer == 0)
        *outputBuffer = new char[reqSize];
    return reqSize;
}
