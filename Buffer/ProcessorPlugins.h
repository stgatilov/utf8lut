#pragma once

class BasePlugin {
public:
	virtual void Pre() {}
	virtual void Post() {}
};
class InputPlugin : public BasePlugin {};
class OutputPlugin : public BasePlugin {};


class ContiguousInput : public InputPlugin {
	BaseBufferProcessor *processor;
	const char *srcBuffer;
	int srcSize;
	int srcDone;
	int chunkSize;

public:
	ContiguousInput(BaseBufferProcessor &owner, const char *buffer, int size) : processor(&owner) {
		srcBuffer = buffer;
		srcSize = size;
		srcDone = 0;
		chunkSize = processor->GetInputBufferRecommendedSize();
		processor->AddPlugin(this);
	}
	virtual void Pre() {
		int bytes = min(srcSize - srcDone, chunkSize);
		processor->SetInputBuffer(srcBuffer + srcDone, bytes);
		processor->SetMode(srcDone + bytes == srcSize);
	}
	virtual void Post() {
		srcDone += processor->GetInputDoneSize();
	}
	bool Finished() const {
		return srcDone == srcSize;
	}
};


class InteractiveInput : public InputPlugin {
	BaseBufferProcessor *processor;
	char *inputBuffer;
	int inputSize;
	int inputSet;
	bool lastBlock;

public:
	InteractiveInput(BaseBufferProcessor &owner) : processor(&owner) {
		inputSize = processor->GetInputBufferRecommendedSize();
		inputBuffer = new char[inputSize];
		inputSet = 0;
		processor->AddPlugin(this);
	}
	~InteractiveInput() {
		delete buffer;
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
		if (inputSet - inputDone > 0)
			memmove(inputBuffer, inputBuffer + inputDone, inputSet - inputDone);
	}
};

class ContiguousOutput : public OutputPlugin {
	BaseBufferProcessor *processor;
	const char *dstBuffer;
	int dstSize;
	int dstDone;

	int streamsCnt, streamOutputSize;
	char *multiBuffer[BaseBufferProcessor::MaxStreamsCount];

public:
	ContiguousOutput(BaseBufferProcessor &owner, char *buffer, int size) : processor(&owner) {
		dstBuffer = buffer;
		dstSize = size;
		dstDone = 0;
		streamsCnt = processor->GetStreamsCount();
		streamOutputSize = 0;
		if (streamsCnt > 1) {
			streamOutputSize = processor->GetOutputBufferMinSize(processor->GetInputBufferRecommendedSize());
			for (int i = 0; i < streamsCnt; i++)
				multiBuffer[i] = new char[streamOutputSize];
		}
		processor->AddPlugin(this);
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
		else
			processor->SetOutputBuffer(dstBuffer + dstDone, dstSize - dstDone);
	}
	virtual void Post() {
		if (streamsCnt > 1) {
			for (int i = 0; i < streamsCnt; i++) {
				int done = processor->GetOutputDoneSize(i);
				assert(dstDone + done <= streamOutputSize);
				memcpy(dstBuffer + dstDone, multiBuffer[i], done);
				dstDone += done;
			}
		}
		else
			dstDone += processor->GetOutputDoneSize();
	}

	int GetTotalOutputSize() const {
		return dstDone;
	}
};

class InteractiveOutput : public OutputPlugin {
	BaseBufferProcessor *processor;
	int streamsCnt, streamOutputSize;
	char *multiBuffer[BaseBufferProcessor::MaxStreamsCount];

public:
	InteractiveOutput(BaseBufferProcessor &owner) : processor(&owner) {
		streamsCnt = processor->GetStreamsCount();
		streamOutputSize = processor->GetOutputBufferMinSize(processor->GetInputBufferRecommendedSize());
		for (int i = 0; i < streamsCnt; i++)
			multiBuffer[i] = new char[streamOutputSize];
		processor->AddPlugin(this);
	}
	~InteractiveOutput() {
		for (int i = 0; i < streamsCnt; i++)
			delete[] multiBuffer[i];
	}
	virtual void Pre() {
		for (int i = 0; i < streamsCnt; i++)
			processor->SetOutputBuffer(multiBuffer[i], streamOutputSize, i);
	}
	void GetBuffer(const char *&buffer, int &size, int index = 0) const {
		buffer = multiBuffer[index];
		size = processor->GetOutputDoneSize(index);
	}
};
