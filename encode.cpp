#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <tmmintrin.h>
#include "PerfDefs.h"
#include <intrin.h>
#include <stdlib.h>
#ifdef TIMING
	#include "BOM_Profiler.h"
#endif

#include "../pipelineTool/iacaMarks.h"

struct LutEntryEncode {
	__m128i shuf;
	__m128i headerMask;
	uint32_t dstStep;
};
static CACHEALIGN LutEntryEncode lutTableEncode[256];
#define LUT_ACCESS(ptr, offset) (const LutEntryEncode *)((const char *)ptr + offset)

template<int MaxBytes, bool CheckExceed, bool Validate, int InputType>
struct EncoderCore {
	FORCEINLINE bool operator()(const char *&ptrSource, char *&ptrDest, const LutEntryEncode *RESTRICT lutTable) {
		static_assert(!Validate || CheckExceed, "Validate core mode requires CheckExceed enabled");
		const char *RESTRICT pSource = ptrSource;
		char *RESTRICT pDest = ptrDest;
		
		__m128i reg = _mm_loadu_si128((const __m128i *)pSource);

		if (MaxBytes == 2) {
			//levels of bytes
			__m128i levelA = reg;													//abcdefgh|ABCDEFGH
			__m128i levelB = _mm_srli_epi16(reg, 6);								//ghABCDEF|GH......
			//put all bytes of each half into a register
			__m128i levBA = _mm_xor_si128(levelB, _mm_slli_epi16(levelA, 8));		//ghABC,,,|abcdefgh

			//check which symbols are long
			__m128i lenGe2 = _mm_cmpgt_epi16(levelB, _mm_set1_epi16(0x0001U));
			//check if there are three+ bytes symbols
			if (CheckExceed && !_mm_cmp_allzero(_mm_cmpgt_epi16(levelB, _mm_set1_epi16(0x001FU))))
				return true;
			//compose lens masks for lookup
			__m128i ctrlMask = _mm_setr_epi8(0, 2, 4, 6, 8, 10, 12, 14, -1, -1, -1, -1, -1, -1, -1, -1);
			__m128i lensAll = _mm_shuffle_epi8(lenGe2, ctrlMask);
			//get indices into lookup table
			uint32_t index = _mm_movemask_epi8(lensAll);
		
			//load info from LUT
			const LutEntryEncode *RESTRICT lookup = LUT_ACCESS(lutTable, index * sizeof(LutEntryEncode));
			//shuffle bytes to compact layout
			__m128i res = _mm_shuffle_epi8(levBA, lookup->shuf);
			//add headers to all bytes
			__m128i header = lookup->headerMask;
			res = _mm_andnot_si128(header, res);
			res = _mm_add_epi8(res, _mm_add_epi8(header, header));
	
			//write results
			_mm_storeu_si128((__m128i *)pDest, res);
			pDest += lookup->dstStep;
		}
		else if (MaxBytes == 3) {
/*			//levels of bytes
			__m128i levelA = reg;
			__m128i levelB = _mm_srli_epi16(reg, 6);
			__m128i levelC = _mm_srli_epi16(reg, 12);
			//put all bytes of each half into a register
			__m128i levAB = _mm_xor_si128(_mm_slli_epi16(levelB, 8), _mm_and_si128(levelA, _mm_set1_epi16(0x00FF)));
			__m128i levels0 = _mm_unpacklo_epi64(levAB, levelC);
			__m128i levels1 = _mm_unpackhi_epi64(levAB, levelC);*/
			//levels of bytes
			__m128i levAC = _mm_maddubs_epi16(_mm_and_si128(reg, _mm_set1_epi16(0xF0FFU)), _mm_set1_epi16(0x1001U));
			__m128i levelB = _mm_srli_epi16(reg, 6);
			//put all bytes of each half into a register
			__m128i levels0 = _mm_unpacklo_epi64(levAC, levelB);
			__m128i levels1 = _mm_unpackhi_epi64(levAC, levelB);

			//check which symbols are long
			__m128i regS = _mm_sub_epi16(reg, _mm_set1_epi16(0x8000U));
			__m128i lenGe2 = _mm_cmpgt_epi16(regS, _mm_set1_epi16(0x807FU));
			__m128i lenGe3 = _mm_cmpgt_epi16(regS, _mm_set1_epi16(0x87FFU));
/*			//compose lens masks for lookup
			__m128i lens0 = _mm_unpacklo_epi64(lenGe2, lenGe3);
			__m128i lens1 = _mm_unpackhi_epi64(lenGe2, lenGe3);
			//make masks compact
			__m128i ctrlMask = _mm_setr_epi8(-1, -1, -1, -1, -1, 0, 2, 4, 6, 8, 10, 12, 14, -1, -1, -1);
			__m128i lens0p = _mm_shuffle_epi8(lens0, ctrlMask);
			__m128i lens1p = _mm_shuffle_epi8(lens1, ctrlMask);
			//get byte offsets into lookup table
			uint32_t offset0 = _mm_movemask_epi8(lens0p);
			uint32_t offset1 = _mm_movemask_epi8(lens1p);*/
			__m128i lensMix = _mm_xor_si128(_mm_slli_epi16(lenGe2, 8), lenGe3);
			uint32_t allMask = _mm_movemask_epi8(lensMix);
			uint32_t offset0 = (allMask & 255U) * 32U;
			uint32_t offset1 = (allMask >> 8U) * 32U;
		
			//load info from LUT
			const LutEntryEncode *RESTRICT lookup0 = LUT_ACCESS(lutTable, offset0); //&lutTable[offset0];
			const LutEntryEncode *RESTRICT lookup1 = LUT_ACCESS(lutTable, offset1); //&lutTable[offset1];
			//shuffle bytes of each half to compact layout
			__m128i res0 = _mm_shuffle_epi8(levels0, lookup0->shuf);
			__m128i res1 = _mm_shuffle_epi8(levels1, lookup1->shuf);
			//add headers to all bytes
			__m128i header0 = lookup0->headerMask;
			__m128i header1 = lookup1->headerMask;
			res0 = _mm_andnot_si128(header0, res0);
			res1 = _mm_andnot_si128(header1, res1);
			res0 = _mm_add_epi8(res0, _mm_add_epi8(header0, header0));
			res1 = _mm_add_epi8(res1, _mm_add_epi8(header1, header1));
	
			//write results
			_mm_storeu_si128((__m128i *)pDest, res0);
			pDest += lookup0->dstStep;
			_mm_storeu_si128((__m128i *)pDest, res1);
			pDest += lookup1->dstStep;
		}

		//save new addresses
		ptrSource += 16;
		ptrDest = pDest;
		return true;
	}
};


char inputBuffer[65536], outputBuffer[65536];

int main() {
	EncoderCore<2, false, false, 2> coder;

	const char *inputPtr = inputBuffer;
	char *outputPtr = outputBuffer;

	for (int i = 0; i < 100000; i++) {
		IACA_VC64_START
		coder(inputPtr, outputPtr, lutTableEncode);
		coder(inputPtr, outputPtr, lutTableEncode);
		coder(inputPtr, outputPtr, lutTableEncode);
		coder(inputPtr, outputPtr, lutTableEncode);
		IACA_VC64_END
	}

	return 0;
}
