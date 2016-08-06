g++ newMain.cpp Base/Timing.cpp Core/DecoderLut.cpp Core/EncoderLut.cpp Buffer/BaseBufferProcessor.cpp -I. --std=c++11 -mssse3 -o utf8_gcc.exe -O3 -D TIMING -D NDEBUG
strip utf8_gcc.exe
