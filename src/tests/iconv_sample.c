#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <time.h>
#include "iconv/iconv.h"

//note: the default MSVC value 512 is inefficient today
#define BUFSIZ (1<<18)

//sample taken from https://www.gnu.org/software/libc/manual/html_node/iconv-Examples.html
//function file2wcs is slightly changed to convert UTF8 to UTF16, plus "result" is global
int result;
int
load_from_utf8_file (int fd, char *outbuf, size_t avail)
{
  char inbuf[BUFSIZ];
  size_t insize = 0;
  char *wrptr = outbuf;
  result = 0;
  iconv_t cd;

  cd = iconv_open ("UTF-16LE", "UTF-8");
  if (cd == (iconv_t) -1)
    {
      /* Something went wrong.  */
      if (errno == EINVAL)
        fprintf(stderr, "conversion from UTF-8 to UTF-16LE not available\n");
      else
        perror ("iconv_open");

      /* Terminate the output string.  */
      *outbuf = '\0';

      return -1;
    }

  while (avail > 0)
    {
      size_t nread;
      size_t nconv;
      const char *inptr = inbuf;

      /* Read more input.  */
      nread = read (fd, inbuf + insize, sizeof (inbuf) - insize);
      if (nread == 0)
        {
          /* When we come here the file is completely read.
             This still could mean there are some unused
             characters in the inbuf.  Put them back.  */
          if (lseek (fd, -(int)insize, SEEK_CUR) == -1)
            result = -1;

          /* Now write out the byte sequence to get into the
             initial state if this is necessary.  */
          iconv (cd, NULL, NULL, &wrptr, &avail);

          break;
        }
      insize += nread;

      /* Do the conversion.  */
      nconv = iconv (cd, &inptr, &insize, &wrptr, &avail);
      if (nconv == (size_t) -1)
        {
          /* Not everything went right.  It might only be
             an unfinished byte sequence at the end of the
             buffer.  Or it is a real problem.  */
          if (errno == EINVAL)
            /* This is harmless.  Simply move the unused
               bytes to the beginning of the buffer so that
               they can be used in the next round.  */
            memmove (inbuf, inptr, insize);
          else
            {
              /* It is a real problem.  Maybe we ran out of
                 space in the output buffer or we have invalid
                 input.  In any case back the file pointer to
                 the position of the last processed byte.  */
              lseek (fd, -(int)insize, SEEK_CUR);
              result = -1;
              break;
            }
        }
    }

  /* Terminate the output string.  */
  if (avail >= sizeof (char))
    *(wrptr) = '\0';

  if (iconv_close (cd) != 0)
    perror ("iconv_close");

  return wrptr - outbuf;
}

#define INPUT_FILE "data_utf8.txt"

int main() {
  int fd = open(INPUT_FILE, O_RDONLY, 0xDEADBEEF);
  if (fd == -1) {
    perror("open");
    fprintf(stderr, "Most likely input file \"" INPUT_FILE "\" is missing.\n");
    return 1;
  }

  struct stat stats;
  fstat(fd, &stats);

  size_t outsize = stats.st_size * 2 + 16;
  char *outbuf = (char*)malloc(outsize);

  int startclock = clock();
  int convsize = load_from_utf8_file(fd, outbuf, outsize);
  int diffclock = clock() - startclock;
  fprintf(stderr, "Elapsed time: %d ms\n", diffclock * 1000 / CLOCKS_PER_SEC);
  if (convsize < 0) {
    fprintf(stderr, "Conversion not possible.\n");
    return 2;
  }
  if (result == 0)
    fprintf(stderr, "Conversion finished successfully.\n");
  else
    fprintf(stderr, "Error happened during conversion.\n");

  unsigned int hash = 0;
  for (int i = 0; i < convsize; i++)
    hash = hash * 31 + outbuf[i];
  fprintf(stderr, "Converted data: size = %d, base-31 poly hash = %08X\n", convsize, hash);
  return 0;
}
