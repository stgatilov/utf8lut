#pragma once

//----------------------------COMPILER-SPECIFIC

#ifdef _MSC_VER
	#define FORCEINLINE __forceinline
	#define NOINLINE __declspec(noinline)
	#define ALIGN(n) __declspec(align(n))
	#define RESTRICT __restrict
#else
	#define FORCEINLINE __attribute__((always_inline)) inline
	#define NOINLINE __attribute__((noinline))
	#define ALIGN(n) __attribute__((aligned(n)))
	#define RESTRICT __restrict__
#endif
#define CACHE_LINE 64
#define CACHEALIGN ALIGN(CACHE_LINE)

//----------------------------COMMON-MACROS

//common macros
#define ALIGNDOWN(x, b) ((x) / (b) * (b))
#define ALIGNUP(x, b) ALIGNDOWN(((x) + (b) - 1), b)
#define TPNT(x, type, offset) ((type *)(((char *)(x)) + offset))
#define TREF(x, type, offset) TPNT(x, type, offset)[0]
#define DMIN(a, b) ((a) < (b) ? (a) : (b))
#define DMAX(a, b) ((a) > (b) ? (a) : (b))

//----------------------------SSE-MACROS

/*#ifdef SSE4	//note: ptest instruction is slower than pmovmskb + cmp
	#include "smmintrin.h"
	#define _mm_cmp_allzero(reg) _mm_test_all_zeros(reg, reg)
	#define _mm_cmp_allone(reg) _mm_test_all_ones(reg)
#else*/
	#define _mm_cmp_allzero(reg) (_mm_movemask_epi8(reg) == 0)
	#define _mm_cmp_allone(reg) (_mm_movemask_epi8(reg) == 0xFFFF)
//#endif
