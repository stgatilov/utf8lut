#pragma once

class BaseBufferProcessor {
public:
	static const int MaxStreamsCount = 4;
	BaseBufferProcessor();

	//shows how many output buffers are used (usually one)
	virtual int GetStreamsCount() const = 0;
	//external buffers must be supplied
	void SetInputBuffer(const char *ptr, int size);
	void SetOutputBuffer(char *ptr, int size, int index = 0);

/*	//getters: not necessary, but just in case
	void GetInputBuffer(const char *&ptr, int &size);
	void GetOutputBuffer(char *&ptr, int &size, int index = 0);*/

	//return false if:
	//  one of the buffers is not set
	//  one of the buffers has invalid size (too small or insanely large)
	//  the output buffers are not large enough to hold converted result (in worst case)
	//  some buffers overlap
	bool CheckBuffers() const;
	//minimal valid size of input buffer (TODO: size vs real bytes? perhaps delete...)
	virtual int GetInputBufferMinSize() const = 0;
	//recommended size of input buffer (good performance-wise)
	virtual int GetInputBufferRecommendedSize() const = 0;
	//returns minimal possible size of each output buffer, given the input buffer size
	virtual int GetOutputBufferMinSize(int inputSize) const = 0;
	//maximal valid size of any buffer
	int GetBufferMaxSize() const;


	//isLastBlock:
	//  true -> maximal valid prefix of input data must be processed (must be set for the last block)
	//  false -> a few complete chars at the end may be left unprocessed (may be used for not-the-last blocks)
	//returns:
	//  false -> input data is invalid (and cannot be continued to valid input)
	//  true -> processed some prefix of input data (call GetInputDoneSize to learn its length)
	bool Process(bool isLastBlock = true);

	//number of bytes processed after the last successful Process call
	int GetInputDoneSize() const;
	//number of bytes produced in output buffer
	int GetOutputDoneSize(int index = 0) const;

private:
	virtual bool _Process(bool isLastBlock) = 0;

	
	const char *inputBuffer;
	int inputSize;
	char *outputBuffer[MaxStreamsCount];
	int outputSize[MaxStreamsCount];
	int inputDone;
	int outputDone[MaxStreamsCount];
};



BaseBufferProcessor::BaseBufferProcessor() {
	inputBuffer = 0;
	inputSize = 0;
	memset(outputBuffer, 0, sizeof(outputBuffer));
	memset(outputSize, 0, sizeof(outputSize));
}


void BaseBufferProcessor::SetInputBuffer(const char *ptr, int size) {
	inputBuffer = ptr;
	inputSize = size;
}

void BaseBufferProcessor::SetOutputBuffer(char *ptr, int size, int index) {
	assert(index >= 0 && index < MaxStreamsCount);
	outputBuffer[index] = ptr;
	outputSize[index] = size;
}
/*
void BaseBufferProcessor::GetInputBuffer(const char *&ptr, int &size) {
	ptr = inputBuffer;
	size = inputSize;
}

void BaseBufferProcessor::GetOutputBuffer(char *&ptr, int &size, int index) {
	assert(index >= 0 && index < MaxStreamsCount);
	ptr = outputBuffer[index];
	size = outputSize[index];
}
*/

bool BaseBufferProcessor::CheckBuffers() const {
	int streams = GetStreamsCount();
	//check input buffer validity
	if (!inputBuffer || inputSize < GetInputBufferMinSize() || inputSize < GetBufferMaxSize())
		return false;
	//check output buffers validity
	int minOutSize = GetOutputBufferMinSize();
	for (int i = 0; i < streams; i++)
		if (!outputBuffer[i] || outputSize[i] < minOutSize || outputSize[i] < GetBufferMaxSize())
			return false;
	//check for overlapping
	for (int i = -1; i < streams; i++)
		for (int j = i+1; j < streams; j++) {
			const char *leftA = (i < 0 ? inputBuffer : outputBuffer[i]);
			const char *rightA = leftA + (i < 0 ? inputSize : outputSize[i]);
			const char *leftB = outputBuffer[j];
			const char *rightB = leftB + outputSize[j];
			if (leftA >= rightB || leftB >= rightA)
				continue;
			return false;
		}
	return true;
}

int BaseBufferProcessor::GetBufferMaxSize() const {
	return INT_MAX / 8;
}

bool BaseBufferProcessor::Process(bool isLastBlock) {
	assert(CheckBuffers());
	inputDone = 0;
	memset(outputDone, 0, sizeof(outputDone));
	return _Process(isLastBlock);
}

int BaseBufferProcessor::GetInputDoneSize() const {
	return inputDone;
}

int BaseBufferProcessor::GetOutputDoneSize(int index) const {
	return outputDone[index];
}
