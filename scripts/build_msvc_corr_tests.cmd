set ROOT=../src/
cl ^
    %ROOT%Core/DecoderLut.cpp ^
    %ROOT%Core/EncoderLut.cpp ^
    %ROOT%Buffer/BaseBufferProcessor.cpp ^
    %ROOT%Message/MessageConverter.cpp ^
    %ROOT%Tests/CorrectnessTests.cpp ^
    /I"../src" /Fe"CorrectnessTests_msvc.exe" ^
    /D _CRT_SECURE_NO_DEPRECATE ^
    /O2 /Oi /W2 /EHsc /FAs /Zi /MD /link/opt:ref
