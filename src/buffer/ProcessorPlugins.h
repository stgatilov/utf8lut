#pragma once

#include "Buffer/BaseBufferProcessor.h"

class BasePlugin {
public:
    virtual ~BasePlugin() {}
    virtual void Pre() {}
    virtual void Post() {}
};
class InputPlugin : public BasePlugin {};
class OutputPlugin : public BasePlugin {};


class ContiguousInput : public InputPlugin {
    BaseBufferProcessor *processor;
    const char *srcBuffer;
    long long srcSize;
    long long srcDone;
    int chunkSize;
    bool finished;

public:
    ContiguousInput(BaseBufferProcessor &owner, const char *buffer, long long size) : processor(&owner) {
        srcBuffer = buffer;
        srcSize = size;
        srcDone = 0;
        chunkSize = processor->GetInputBufferRecommendedSize();
        finished = (srcSize == 0);
        processor->AddPlugin(*this);
    }
    virtual void Pre() {
        assert(!finished);
        int bytes = GetNextChunkSize();
        processor->SetInputBuffer(srcBuffer + srcDone, bytes);
        finished = (srcDone + bytes == srcSize);
        processor->SetMode(finished);
    }
    virtual void Post() {
        srcDone += processor->GetInputDoneSize();
    }
    long long GetProcessedInputSize() const {
        return srcDone;
    }
    long long GetRemainingDataSize() const {
        return srcSize - srcDone;
    }
    long long GetBufferSize() const {
        return srcSize;
    }
    int GetNextChunkSize() const {
        long long bytes = srcSize - srcDone;
        if (bytes > chunkSize)
            bytes = chunkSize;
        return int(bytes);
    }
    bool Finished() const {
        return finished;
    }
};


class InteractiveInput : public InputPlugin {
    BaseBufferProcessor *processor;
    char *inputBuffer;
    int inputSize;
    int inputSet;
    bool lastBlock;
    long long totalSizeDone;

public:
    InteractiveInput(BaseBufferProcessor &owner) : processor(&owner) {
        inputSize = processor->GetInputBufferRecommendedSize();
        inputBuffer = new char[inputSize];
        inputSet = 0;
        totalSizeDone = 0;
        processor->AddPlugin(*this);
    }
    ~InteractiveInput() {
        delete[] inputBuffer;
    }
    void GetBuffer(char *&buffer, int &size) const {
        buffer = inputBuffer + inputSet;
        size = inputSize - inputSet;
    }
    void ConfirmInputBytes(int bytes, bool isLastBlock = true) {
        inputSet += bytes;
        lastBlock = isLastBlock;
    }
    virtual void Pre() {
        processor->SetInputBuffer(inputBuffer, inputSet);
        processor->SetMode(lastBlock);
    }
    virtual void Post() {
        int inputDone = processor->GetInputDoneSize();
        int remains = inputSet - inputDone;
        if (remains > 0)
            memmove(inputBuffer, inputBuffer + inputDone, remains);
        inputSet = remains;
        totalSizeDone += inputDone;
    }
    int GetRemainingDataSize() const {
        return inputSet;
    }
    long long GetProcessedInputSize() const {
        return totalSizeDone;
    }
};

class ContiguousOutput : public OutputPlugin {
    BaseBufferProcessor *processor;
    char *dstBuffer;
    long long dstSize;
    long long dstDone;
    int maxSize;

    int streamsCnt, streamOutputSize;
    char *multiBuffer[BaseBufferProcessor::MaxStreamsCount];

public:
    static long long GetMaxOutputSize(const BaseBufferProcessor &processor, long long inputSize) {
        return processor.GetStreamsCount() * processor.GetOutputBufferMinSize(inputSize);
    }
    ContiguousOutput(BaseBufferProcessor &owner, char *buffer, long long size) : processor(&owner) {
        dstBuffer = buffer;
        dstSize = size;
        dstDone = 0;
        streamsCnt = processor->GetStreamsCount();
        streamOutputSize = 0;
        maxSize = processor->GetBufferMaxSize();
        if (streamsCnt > 1) {
            streamOutputSize = (int)processor->GetOutputBufferMinSize(processor->GetInputBufferRecommendedSize());
            for (int i = 0; i < streamsCnt; i++)
                multiBuffer[i] = new char[streamOutputSize];
        }
        processor->AddPlugin(*this);
    }
    ~ContiguousOutput() {
        if (streamsCnt > 1)
            for (int i = 0; i < streamsCnt; i++)
                delete[] multiBuffer[i];
    }
    virtual void Pre() {
        if (streamsCnt > 1) {
            for (int i = 0; i < streamsCnt; i++)
                processor->SetOutputBuffer(multiBuffer[i], streamOutputSize, i);
        }
        else {
            long long remains = dstSize - dstDone;
            if (remains > maxSize)
                remains = maxSize;
            processor->SetOutputBuffer(dstBuffer + dstDone, int(remains));
        }
    }
    virtual void Post() {
        if (streamsCnt > 1) {
            for (int i = 0; i < streamsCnt; i++) {
                int done = processor->GetOutputDoneSize(i);
                assert(dstDone + done <= dstSize);
                memcpy(dstBuffer + dstDone, multiBuffer[i], done);
                dstDone += done;
            }
        }
        else
            dstDone += processor->GetOutputDoneSize();
    }
    long long GetFilledOutputSize() const {
        return dstDone;
    }
    long long GetBufferSize() const {
        return dstSize;
    }
};

class InteractiveOutput : public OutputPlugin {
    BaseBufferProcessor *processor;
    int streamsCnt, streamOutputSize;
    char *multiBuffer[BaseBufferProcessor::MaxStreamsCount];
    long long totalSizeDone;

public:
    InteractiveOutput(BaseBufferProcessor &owner) : processor(&owner) {
        streamsCnt = processor->GetStreamsCount();
        streamOutputSize = (int)processor->GetOutputBufferMinSize(processor->GetInputBufferRecommendedSize());
        for (int i = 0; i < streamsCnt; i++)
            multiBuffer[i] = new char[streamOutputSize];
        totalSizeDone = 0;
        processor->AddPlugin(*this);
    }
    ~InteractiveOutput() {
        for (int i = 0; i < streamsCnt; i++)
            delete[] multiBuffer[i];
    }
    virtual void Pre() {
        for (int i = 0; i < streamsCnt; i++)
            processor->SetOutputBuffer(multiBuffer[i], streamOutputSize, i);
    }
    virtual void Post() {
        for (int i = 0; i < streamsCnt; i++)
            totalSizeDone += processor->GetOutputDoneSize(i);
    }
    int GetStreamsCount() const {
        return streamsCnt;
    }
    void GetBuffer(const char *&buffer, int &size, int index = 0) const {
        buffer = multiBuffer[index];
        size = processor->GetOutputDoneSize(index);
    }
    long long GetFilledOutputSize() const {
        return totalSizeDone;
    }
};
