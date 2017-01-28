#pragma once

#include "Buffer/BufferDecoder.h"
#include "Buffer/BufferEncoder.h"

//metaprogramming helper
template<bool Condition, class First, class Second> struct TernaryOperator {};
template<class First, class Second> struct TernaryOperator<true, First, Second> { typedef First Type; };
template<class First, class Second> struct TernaryOperator<false, First, Second> { typedef Second Type; };

//possible formats of input/output data
enum DataFormat {
    dfUtf8,
    dfUtf16,
    dfUtf32,
    dfUtfCount    //helper
};


//modes of conversion (synchronized with EncoderMode and DecoderMode)
//TODO: full explanations here!
enum ConversionMode {
    cmFast = 0,
    cmFull = 1,
    cmValidate = 2
};

//This selector can be used to get type of processor by options, e.g.:
//  typedef ProcessorSelector<0, 1>::WithOptions<cmValidate, 2>::Processor MyProcessor;
//  BaseBufferProcessor *processor = new MyProcessor();
//TODO: more explanations, all options, what about error handlers?
template<int SrcFormat, int DstFormat>
struct ProcessorSelector {
    static_assert(SrcFormat >= 0 && SrcFormat < dfUtfCount, "Unsupported format");
    static_assert(DstFormat >= 0 && DstFormat < dfUtfCount, "Unsupported format");
    static_assert((SrcFormat == dfUtf8) != (DstFormat == dfUtf8), "Supported only conversions: from UTF-8 or to UTF-8");

    template<int Mode = cmValidate, int MaxBytes = 3, int SpeedMult = 1>
    struct WithOptions {
        typedef typename TernaryOperator<SrcFormat == dfUtf8,
            BufferDecoder<MaxBytes, (DstFormat == dfUtf32 ? 4 : 2), Mode, SpeedMult>,
            BufferEncoder<MaxBytes, (SrcFormat == dfUtf32 ? 4 : 2), Mode, SpeedMult>
        >::Type Processor;

        static Processor* Create(int *errorCounter = 0);
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

FORCEINLINE int GetUnitSizeOfFormat(int format) { return format == dfUtf8 ? 1 : format == dfUtf16 ? 2 : 4; }

template<int SrcFormat, int DstFormat>
bool ProcessorSelector<SrcFormat, DstFormat>::OnErrorMissCodeUnits(
    void *context, const char *&srcBuffer, int srcBytes, char *&dstBuffer, int dstBytes
) {
    int &counter = *(int*)context;
    srcBuffer += GetUnitSizeOfFormat(SrcFormat);
    counter++;
    return true;
}

template<int SrcFormat, int DstFormat>
bool ProcessorSelector<SrcFormat, DstFormat>::OnErrorSetReplacementChars(
    void *context, const char *&srcBuffer, int srcBytes, char *&dstBuffer, int dstBytes
) {
    int &counter = *(int*)context;
    srcBuffer += GetUnitSizeOfFormat(SrcFormat);
    counter++;
    if (DstFormat == dfUtf8) {
        if (dstBytes < 3) return false;
        *dstBuffer++ = (char)0xEF;
        *dstBuffer++ = (char)0xBF;
        *dstBuffer++ = (char)0xBD;
    }
    else if (DstFormat == dfUtf16) {
        if (dstBytes < 2) return false;
        *dstBuffer++ = (char)0xFD;
        *dstBuffer++ = (char)0xFF;
    }
    else if (DstFormat == dfUtf32) {
        if (dstBytes < 4) return false;
        *dstBuffer++ = (char)0xFD;
        *dstBuffer++ = (char)0xFF;
        *dstBuffer++ = (char)0x00;
        *dstBuffer++ = (char)0x00;
    }
    return true;
}

template<int SrcFormat, int DstFormat>
template<int Mode, int MaxBytes, int SpeedMult>
typename ProcessorSelector<SrcFormat, DstFormat>::WithOptions<Mode, MaxBytes, SpeedMult>::Processor *
ProcessorSelector<SrcFormat, DstFormat>::WithOptions<Mode, MaxBytes, SpeedMult>::Create(int *errorCounter) {
    Processor *result = new Processor();
    if (errorCounter)
        result->SetErrorCallback(OnErrorSetReplacementChars, errorCounter);
    return result;
}
