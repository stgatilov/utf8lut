g++ utf8.cpp Base/Timing.cpp Core/DecoderLut.cpp Core/EncoderLut.cpp -I. -O2 --std=c++11 -mssse3 -D TIMING -D NDEBUG -o utf8_gcc.exe