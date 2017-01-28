set ROOT=../src/
g++ ^
    %ROOT%Base/Timing.cpp ^
    %ROOT%Core/DecoderLut.cpp ^
    %ROOT%Core/EncoderLut.cpp ^
    %ROOT%Buffer/BaseBufferProcessor.cpp ^
    %ROOT%Buffer/AllProcessors.cpp ^
    %ROOT%Message/MessageConverter.cpp ^
    %ROOT%Tests/FileConverter.cpp ^
    -I"../src" -o"FileConverter_mingw.exe" ^
    -D NDEBUG -D TIMING ^
    --std=c++11 -mssse3 -O3

strip FileConverter_mingw.exe
