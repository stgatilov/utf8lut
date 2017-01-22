set ROOT=../src/
g++ ^
    %ROOT%Core/DecoderLut.cpp ^
    %ROOT%Core/EncoderLut.cpp ^
    %ROOT%Buffer/BaseBufferProcessor.cpp ^
    %ROOT%Message/MessageConverter.cpp ^
    %ROOT%Tests/CorrectnessTests.cpp ^
    -I"../src" -o"CorrectnessTests_mingw.exe" ^
    --std=c++11 -mssse3 -O3

strip CorrectnessTests_mingw.exe
