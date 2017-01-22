set ROOT=../src/
g++ ^
    %ROOT%Core/DecoderLut.cpp ^
    %ROOT%Core/EncoderLut.cpp ^
    %ROOT%Buffer/BaseBufferProcessor.cpp ^
    %ROOT%iconv/iconv.cpp ^
    -I"../src" -shared -o"iconv_u8l_mingw.dll" ^
    -D NDEBUG -D ICONV_UTF8LUT_BUILD ^
    --std=c++11 -mssse3 -O3

strip iconv_u8l_mingw.dll
