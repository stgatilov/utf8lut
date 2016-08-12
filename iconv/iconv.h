#ifndef ICONV_UTF8LUT_HEADER
#define ICONV_UTF8LUT_HEADER

//cross-platform export/import macro for shared library
#ifdef _WIN32
	#ifdef ICONV_UTF8LUT_BUILD
		#define ICONV_UTF8LUT_EXPORT __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
	#else
		#define ICONV_UTF8LUT_EXPORT __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
	#endif
#else
	#define ICONV_UTF8LUT_EXPORT __attribute__ ((visibility ("default")))
#endif

//global var "errno" used to report errors
#include <errno.h>
//any valid iconv_t descriptor is actually a pointer to BaseBufferProcessor object
typedef void *iconv_t;

#ifdef __cplusplus
extern "C" {
#endif

	// see manual at http://man7.org/linux/man-pages/man3/iconv_open.3.html
	// Supported conversion modes are: from UTF-8 to (UTF-16 or UTF-32) and vice versa.
	//
	ICONV_UTF8LUT_EXPORT iconv_t iconv_open(const char *tocode, const char *fromcode);

	// see manual at http://man7.org/linux/man-pages/man3/iconv.3.html
	//
	// Breaking differences from the official iconv interface are:
	//  1. If input is fully converted without errors, then just "1" is returned (instead of number of chars converted).
	//  2. If output buffer is not big enough to hold converted data for any valid input of size *inbytesleft,
	//     then E2BIG verdict may be returned with considerable amount of input data left unprocessed (usually up to 64KB)
	//     (original interface guarantees that maximal possible number of input charactars is processed).
	//
	// Interface extension:
	//  3. If inbuf and *inbuf are NOT null, and either outbuf or *outbuf is null, then:
	//     maximal possible size of converted output for input data of size *inbytesleft is stored into *outbytesleft.
	//     Note that inbuf parameter is NOT used in this case.
	//
	// In order to avoid E2BIG verdict due to point 2, you should make sure that output buffer is large enough.
	// This can be easily achieved using point 3:
	//     size_t inbufsize = {...}, outbufsize;
	//     char *outbuf = (char*)0xDEADBEEF;
	//     iconv(
	//       cd,           //conversion descriptor
	//       &outbuf,      //not used, but must be non-NULL
	//       &inbufsize,   //size of input buffer
	//       NULL,         //must be NULL
	//       &outbufsize   //correct size of output buffer would be here
	//     );
	//     outbuf = malloc(outbufsize);
	//     ...
	//     char *inbuf = {...};
	//     iconv(cd, &inbuf, &inbufsize, &outbuf, &outbufsize);
	//     ...
	//	
	ICONV_UTF8LUT_EXPORT size_t iconv(iconv_t cd, const char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft);

	// see manual at http://man7.org/linux/man-pages/man3/iconv_close.3.html
	//
	ICONV_UTF8LUT_EXPORT int iconv_close(iconv_t cd);

#ifdef __cplusplus
}
#endif

#endif
