set ROOT=../src/
cl ^
    %ROOT%Core/DecoderLut.cpp ^
    %ROOT%Core/EncoderLut.cpp ^
    %ROOT%Buffer/BaseBufferProcessor.cpp ^
    %ROOT%iconv/iconv.cpp ^
    /I"../src" /LD /Fe"iconv_u8l_msvc.dll" ^
    /D _CRT_SECURE_NO_DEPRECATE ^
    /D NDEBUG /D ICONV_UTF8LUT_BUILD ^
    /O2 /Oi /W2 /EHsc /FAs /Zi /MD /link/opt:ref
