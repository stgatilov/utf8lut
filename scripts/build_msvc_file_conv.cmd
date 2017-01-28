set ROOT=../src/
cl ^
    %ROOT%Base/Timing.cpp ^
    %ROOT%Core/DecoderLut.cpp ^
    %ROOT%Core/EncoderLut.cpp ^
    %ROOT%Buffer/BaseBufferProcessor.cpp ^
    %ROOT%Buffer/AllProcessors.cpp ^
    %ROOT%Message/MessageConverter.cpp ^
    %ROOT%Tests/FileConverter.cpp ^
    /I"../src" /Fe"FileConverter_msvc.exe" ^
    /D _CRT_SECURE_NO_DEPRECATE ^
    /D NDEBUG /D TIMING ^
    /O2 /Oi /W2 /EHsc /FAs /Zi /MD /link/opt:ref
