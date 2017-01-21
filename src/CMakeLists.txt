CMAKE_MINIMUM_REQUIRED (VERSION 3.2.1)

PROJECT(utf8lut)

#=================================================

SET (base_sources
    base/PerfDefs.h
    base/Timing.cpp
    base/Timing.h
)
SET (core_sources
    core/DecoderLut.cpp
    core/DecoderLut.h
    core/DecoderProcess.h
    core/Dfa.h
    core/EncoderLut.cpp
    core/EncoderLut.h
    core/EncoderProcess.h
    core/ProcessTrivial.h
)
SET (buffer_sources
    buffer/BaseBufferProcessor.cpp
    buffer/BaseBufferProcessor.h
    buffer/BufferDecoder.h
    buffer/BufferEncoder.h
    buffer/ProcessorPlugins.h
)
SET (message_sources
    message/MessageConverter.cpp
    message/MessageConverter.h
)

INCLUDE_DIRECTORIES(.)

#=================================================

ADD_EXECUTABLE (CorrectnessTests 
    ${base_sources}
    ${core_sources}
    ${buffer_sources}
    ${message_sources}
    tests/CorrectnessTests.cpp
)

ADD_LIBRARY (iconv_u8l SHARED
    ${base_sources}
    ${core_sources}
    ${buffer_sources}
    iconv/iconv.cpp
    iconv/iconv.h
)

ADD_EXECUTABLE (iconv_sample
    iconv/iconv.h
    tests/iconv_sample.c
)
TARGET_LINK_LIBRARIES (iconv_sample iconv_u8l)

