#include "Message/MessageConverter.h"
#include <stdio.h>

ConversionResult ConvertInMemory(BaseBufferProcessor &processor, const char *inputBuffer, long long inputSize, char *outputBuffer, long long outputSize) {
    ConversionResult result;
    result.status = (ConversionStatus)-1;
    result.inputSize = 0;
    result.outputSize = 0;
    
    if (inputSize < 0 || !inputBuffer && inputSize != 0) {
        result.status = csInputOutputNoAccess;
        return result;
    }
    if (outputSize < 0 || !outputBuffer && outputSize != 0) {
        result.status = csInputOutputNoAccess;
        return result;
    }

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
            break;
        }
    }

    result.inputSize = input.GetProcessedInputSize();
    result.outputSize = output.GetFilledOutputSize();

    if (result.status == csIncorrectData) {
        //hard error occured inside loop
    }
    else if (input.GetRemainingDataSize() != 0) {
        //some bytes remain in the input
        result.status = csIncompleteData;
    }
    else {
        //everything is OK
        result.status = csSuccess;
    }

    return result;
}

long long ConvertInMemorySize(BaseBufferProcessor &processor, long long inputSize, char **outputBuffer) {
    long long reqSize = ContiguousOutput::GetMaxOutputSize(processor, inputSize);
    if (outputBuffer && *outputBuffer == 0)
        *outputBuffer = new char[reqSize];
    return reqSize;
}


ConversionResult ConvertFile(BaseBufferProcessor &processor, const char *inputFilePath, const char *outputFilePath, ConvertFilesSettings settings) {
    ConversionResult result;
    result.status = (ConversionStatus)-1;
    result.inputSize = 0;
    result.outputSize = 0;

    if (!inputFilePath || !outputFilePath) {
        result.status = csInputOutputNoAccess;
        return result;
    }

    FILE *fin = fopen(inputFilePath, "rb");
    FILE *fout = fopen(outputFilePath, "wb");
    if (!fin || !fout) {
        result.status = csInputOutputNoAccess;
        if (fin) fclose(fin);
        if (fout) fclose(fout);
        return result;
    }

    processor.Clear();
    InteractiveInput input(processor);
    InteractiveOutput output(processor);

    //ask how many output buffers are there
    int streamsCnt = output.GetStreamsCount();

    while (!feof(fin)) {
        //ask where to write input data
        char *inputBuffer;
        int maxSize;
        input.GetBuffer(inputBuffer, maxSize);
        //read bytes from input
        int readSize = (int)fread(inputBuffer, 1, maxSize, fin);
        //tell how many bytes we really have
        input.ConfirmInputBytes(readSize, !!feof(fin));

        //do all the work
        bool ok = processor.Process();

        //write all the properly converted data to output
        for (int k = 0; k < streamsCnt; k++) {
            //get output bytes
            const char *outputBuffer;
            int outSize;
            output.GetBuffer(outputBuffer, outSize, k);
            //write them to file
            fwrite(outputBuffer, 1, outSize, fout);
        }

        //check if hard error occurred
        if (!ok) {
            result.status = csIncorrectData;
            break;
        }
    }

    result.inputSize = input.GetProcessedInputSize();
    result.outputSize = output.GetFilledOutputSize();

    if (result.status == csIncorrectData) {
        //hard error happened in the loop
    }
    else if (input.GetRemainingDataSize() != 0) {
        //some bytes in the input remain
        result.status = csIncompleteData;
    }
    else {
        //everything is OK
        result.status = csSuccess;
    }

    fclose(fin);
    fclose(fout);

    return result;
}
