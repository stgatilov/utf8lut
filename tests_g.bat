g++ Tests/CorrectnessTests.cpp Base/Timing.cpp Core/DecoderLut.cpp Core/EncoderLut.cpp Buffer/BaseBufferProcessor.cpp Message/MessageConverter.cpp -I. --std=c++11 -mssse3 -o tests_gcc.exe -O3
strip tests_gcc.exe
