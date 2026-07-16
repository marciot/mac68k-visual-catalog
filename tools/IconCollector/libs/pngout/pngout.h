#include "stdint.h"
#include <stdlib.h>

#define USE_MINIZ 9

#include "pngout_miniz.h"

struct pngout {
  uint8_t output[64 + MINIZ_BUFFER_SIZE]; /* output buffer */
  size_t nout;          /* number of bytes to output */

  uint16_t w, h;        /* width, height */
  size_t bpl;           /* bytes per line */
  uint32_t crc32;       /* running CRC32 */
  uint16_t s1, s2;      /* Adler CRC sums */
  uint16_t i, j;        /* i : column, j : row */
  uint8_t grayscale;    /* if non-zero, image is grayscale */
};

void pngout_start(struct pngout *s, uint16_t width, uint16_t height, uint8_t grayscale);
void pngout_transparent_rgb(struct pngout *s, uint8_t r, uint8_t g, uint8_t b);
void pngout_rgb(struct pngout *s, uint8_t r, uint8_t g, uint8_t b);
void pngout_end(struct pngout *s);
void pngout_utf8_text(struct pngout *s, char *keyword, char *utf8_str);
Boolean pngout_has_data(struct pngout *s, size_t *howMuch);