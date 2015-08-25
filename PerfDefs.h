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
#define TREF(x, type, offset) ((type *)(((char *)(x)) + offset))[0]
#define DMIN(a, b) ((a) < (b) ? (a) : (b))
#define DMAX(a, b) ((a) > (b) ? (a) : (b))
