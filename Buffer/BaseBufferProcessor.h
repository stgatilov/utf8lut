#pragma once

#include <string.h>
#include <assert.h>

class BasePlugin;

class BaseBufferProcessor {
public:
	static const int MaxStreamsCount = 4;
	static const int MaxPluginsCount = 2;
	BaseBufferProcessor();
	virtual ~BaseBufferProcessor();

	//reset to default state
	void Clear();

	//shows how many output buffers are used (usually one)
	virtual int GetStreamsCount() const = 0;
	//external buffers must be supplied
	void SetInputBuffer(const char *ptr, int size);
	void SetOutputBuffer(char *ptr, int size, int index = 0);

	//return false if:
	//  one of the buffers is not set
	//  one of the buffers has invalid size (empty or insanely large)
	//  the output buffers are not large enough to hold converted result (in worst case)
	//  some buffers overlap
	bool CheckBuffers() const;
	//recommended size of input buffer (good performance-wise)
	virtual int GetInputBufferRecommendedSize() const = 0;
	//returns minimal possible size of each output buffer, given the input buffer size
	//note: argument may exceed maximal allowed buffer size
	virtual long long GetOutputBufferMinSize(long long inputSize) const = 0;
	//maximal valid size of any buffer
	int GetBufferMaxSize() const;


	//isLastBlock:
	//  true -> maximal valid prefix of input data must be processed (must be set for the last block)
	//  false -> a few complete chars at the end may be left unprocessed (may be used for not-the-last blocks)
	void SetMode(bool isLastBlock = true);

	//returns:
	//  false -> input data is invalid (and cannot be continued to valid input)
	//  true -> processed some prefix of input data (call GetInputDoneSize to learn its length)
	bool Process();

	//number of bytes processed after the last successful Process call
	int GetInputDoneSize() const;
	//number of bytes produced in output buffer
	int GetOutputDoneSize(int index = 0) const;

	//add input/output plugins to make some things easier
	void AddPlugin(BasePlugin &addedPlugin);

private:
	virtual bool _Process() = 0;

	BasePlugin *plugins[MaxPluginsCount];
	int pluginsCount;

protected:	//accessed in implementations
	const char *inputBuffer;
	int inputSize;
	char *outputBuffer[MaxStreamsCount];
	int outputSize[MaxStreamsCount];
	int inputDone;
	int outputDone[MaxStreamsCount];
	bool lastBlockMode;
};
