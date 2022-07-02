#include "message/MessageConverter.h"
#include <stdio.h>

//=====================================================================================================

ConversionResult ConvertInMemory(BaseBufferProcessor &processor, const char *inputBuffer, long long inputSize, char *outputBuffer, long long outputSize) {
    ConversionResult result;
    result.status = (ConversionStatus)-1;
    result.inputSize = 0;
    result.outputSize = 0;

    if (inputSize < 0 || (!inputBuffer && inputSize != 0)) {
        result.status = csInputOutputNoAccess;
        return result;
    }
    if (outputSize < 0 || (!outputBuffer && outputSize != 0)) {
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
        //hard error occurred inside loop
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

//=====================================================================================================

#if defined(_WIN32)
#define _WIN32_WINNT 0x502
#include "windows.h"

ConversionResult ConvertFile_MemoryMappedWhole(BaseBufferProcessor &processor, const char *inputFilePath, const char *outputFilePath, ConvertFilesSettings settings) {
#define CHECK_FOR_ERROR(cond) \
    if (cond) { \
        result.status = csInputOutputNoAccess; \
        goto end; \
    } \

    GetLastError();
    assert(settings.type == ftMemoryMapWhole);
    ConversionResult result;
    result.status = (ConversionStatus)-1;
    result.inputSize = 0;
    result.outputSize = 0;

    HANDLE hInFile = INVALID_HANDLE_VALUE, hOutFile = INVALID_HANDLE_VALUE;
    HANDLE hInMap = NULL, hOutMap = NULL;
    LPVOID pInView = NULL, pOutView = NULL;
    LARGE_INTEGER szIn, szOut;
    BOOL sizeOk = FALSE;

    hInFile = CreateFileA(inputFilePath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    CHECK_FOR_ERROR(hInFile == INVALID_HANDLE_VALUE);
    sizeOk = GetFileSizeEx(hInFile, &szIn);
    CHECK_FOR_ERROR(!sizeOk);
    //TODO: szIn == NULL?
    hInMap = CreateFileMappingA(hInFile, NULL, PAGE_READONLY, 0, 0, NULL);
    CHECK_FOR_ERROR(hInMap == NULL);
    pInView = MapViewOfFile(hInMap, FILE_MAP_READ, 0, 0, 0);
    CHECK_FOR_ERROR(pInView == NULL);

    szOut.QuadPart = ConvertInMemorySize(processor, szIn.QuadPart);
    hOutFile = CreateFileA(outputFilePath, GENERIC_WRITE | GENERIC_READ , 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    CHECK_FOR_ERROR(hOutFile == INVALID_HANDLE_VALUE);
    hOutMap = CreateFileMappingA(hOutFile, NULL, PAGE_READWRITE, szOut.HighPart, szOut.LowPart, NULL);
    CHECK_FOR_ERROR(hOutMap == NULL);
    pOutView = MapViewOfFile(hOutMap, FILE_MAP_WRITE, 0, 0, 0);
    CHECK_FOR_ERROR(pOutView == NULL);

    //pOutView = new char[szOut.QuadPart];
    result = ConvertInMemory(processor, (const char*)pInView, szIn.QuadPart,(char*)pOutView, szOut.QuadPart);
    //delete pOutView; pOutView = NULL;
end:
    DWORD err = GetLastError();
    if (pInView != NULL)
        UnmapViewOfFile(pInView);
    if (hInMap != NULL && hInMap != INVALID_HANDLE_VALUE)
        CloseHandle(hInMap);
    if (hInFile != NULL && hInFile != INVALID_HANDLE_VALUE)
        CloseHandle(hInFile);
    if (pOutView != NULL)
        UnmapViewOfFile(pOutView);
    if (hOutMap != NULL && hOutMap != INVALID_HANDLE_VALUE)
        CloseHandle(hOutMap);
    if (result.status != csInputOutputNoAccess) {
        szOut.QuadPart = result.outputSize;
        SetFilePointer(hOutFile, szOut.LowPart, &szOut.HighPart, FILE_BEGIN);
        SetEndOfFile(hOutFile);
    }
    if (hOutFile != NULL && hOutFile != INVALID_HANDLE_VALUE)
        CloseHandle(hOutFile);
#undef CHECK_FOR_ERROR

    return result;
}

#else

ConversionResult ConvertFile_MemoryMappedWhole(BaseBufferProcessor &processor, const char *inputFilePath, const char *outputFilePath, ConvertFilesSettings settings) {
    ConversionResult result;
    result.status = csNotImplemented;
    result.inputSize = 0;
    result.outputSize = 0;

    return result;
}

#endif

//=====================================================================================================

ConversionResult ConvertFile(BaseBufferProcessor &processor, const char *inputFilePath, const char *outputFilePath, ConvertFilesSettings settings) {
    if (settings.type == ftMemoryMapWhole)
        return ConvertFile_MemoryMappedWhole(processor, inputFilePath, outputFilePath, settings);

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
