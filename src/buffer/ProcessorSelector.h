#pragma once

#include "Buffer/BufferDecoder.h"
#include "Buffer/BufferEncoder.h"

//metaprogramming helper
template<bool Condition, class First, class Second> struct TernaryOperator {};
template<class First, class Second> struct TernaryOperator<true, First, Second> { typedef First Type; };
template<class First, class Second> struct TernaryOperator<false, First, Second> { typedef First Type; };

//modes of conversion (synchronized with EncoderMode and DecoderMode)
//TODO: full explanations here!
enum ConversionMode {
    cmFast = 0,
    cmFull = 1,
    cmValidate = 2
};

//This selector can be used to get type of processor by options, e.g.:
//  typedef ProcessorSelector<1, 2>::WithOptions<cmValidate, 2>::Processor MyProcessor;
//  BaseBufferProcessor *processor = new MyProcessor();
//TODO: more explanations, all options, what about error handlers?
template<int SrcUnitSize, int DstUnitSize>
struct ProcessorSelector {
    static_assert(SrcUnitSize == 1 || SrcUnitSize == 2 || SrcUnitSize == 4, "Supported unit sizes: UTF-8 = 1, UTF-16 = 2, UTF-32 = 4");
    static_assert(DstUnitSize == 1 || DstUnitSize == 2 || DstUnitSize == 4, "Supported unit sizes: UTF-8 = 1, UTF-16 = 2, UTF-32 = 4");
    static_assert((SrcUnitSize == 1) != (DstUnitSize == 1), "Supported only conversions: from UTF-8 or to UTF-8");

    template<int Mode = cmValidate, int MaxBytes = 3, int SpeedMult = 1>
    struct WithOptions {
        typedef typename TernaryOperator<SrcUnitSize == 1,
            BufferDecoder<MaxBytes, DstUnitSize, Mode, SpeedMult>,
            BufferEncoder<MaxBytes, SrcUnitSize, Mode, SpeedMult>
        >::Type Processor;
    };

    //These functions can be used to BaseBufferProcessor::SetErrorCallback to force conversion of invalid input:
    //  code units are simply skipped until conversion can be continued
    static bool OnErrorMissCodeUnits(void *context, const char *&srcBuffer, int srcBytes, char *&dstBuffer, int dstBytes);
    //  code units are replaced with 0xFFFD code points until conversion can be continued
    static bool OnErrorSetReplacementChars(void *context, const char *&srcBuffer, int srcBytes, char *&dstBuffer, int dstBytes);
};



//==============================
// implementation of some stuff
//==============================

template<int SrcUnitSize, int DstUnitSize>
bool ProcessorSelector<SrcUnitSize, DstUnitSize>::OnErrorMissCodeUnits(
    void *context, const char *&srcBuffer, int srcBytes, char *&dstBuffer, int dstBytes
) {
    int &counter = *(int*)context;
    srcBuffer += SrcUnitSize;
    counter++;
    return true;
}

template<int SrcUnitSize, int DstUnitSize>
bool ProcessorSelector<SrcUnitSize, DstUnitSize>::OnErrorSetReplacementChars(
    void *context, const char *&srcBuffer, int srcBytes, char *&dstBuffer, int dstBytes
) {
    int &counter = *(int*)context;
    srcBuffer += SrcUnitSize;
    counter++;
    if (DstUnitSize == 1) {
        if (dstBytes < 3) return false;
        *dstBuffer++ = (char)0xEF;
        *dstBuffer++ = (char)0xBF;
        *dstBuffer++ = (char)0xBD;
    }
    else if (DstUnitSize == 2) {
        if (dstBytes < 2) return false;
        *dstBuffer++ = (char)0xFD;
        *dstBuffer++ = (char)0xFF;
    }
    else if (DstUnitSize == 4) {
        if (dstBytes < 4) return false;
        *dstBuffer++ = (char)0xFD;
        *dstBuffer++ = (char)0xFF;
        *dstBuffer++ = (char)0x00;
        *dstBuffer++ = (char)0x00;
    }
    return true;
}
