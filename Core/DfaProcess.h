#pragma once

#include <stdint.h>
#include "Base/PerfDefs.h"

namespace DfaUtf8 {
	#include "Dfa.h"
}

template<int OutputType>
FORCEINLINE bool DecodeTrivial(const char *&pSource, char *&pDest, const char *pEnd) {
	using namespace DfaUtf8;
	assert(pSource <= pEnd);

	const uint8_t *RESTRICT s = (const uint8_t *)pSource;
	uint16_t *RESTRICT d = (uint16_t *)pDest;
	uint32_t codepoint;
	uint32_t state = 0;

	const uint8_t *ans_s = s;
	uint16_t *ans_d = d;

	while (s < (const uint8_t *)pEnd) {
		if (decode(&state, &codepoint, *s++))
			continue;
		if (OutputType == 2) {
			if (codepoint > 0xffff) {
				*d++ = (uint16_t)(0xD7C0 + (codepoint >> 10));
				*d++ = (uint16_t)(0xDC00 + (codepoint & 0x3FF));
			} else {
				*d++ = (uint16_t)codepoint;
			}
		}
		else {
			*(uint32_t *)d = codepoint;
			d += 2;
		}
		if (state == UTF8_ACCEPT) {
			ans_s = s;
			ans_d = d;
		}
	}

	pSource = (const char *)ans_s;
	pDest = (char *)ans_d;
	return state != UTF8_REJECT;
}
