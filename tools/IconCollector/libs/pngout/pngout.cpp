/*
 * This is a minimal encoder for uncompressed PNGs.
 *
 * It is based on this Python implementation:
 *   http://mainisusuallyafunction.blogspot.com/search/label/png
 */

#include <stdio.h>
#include <string.h>

#include "pngout.h"

#include "pngout_miniz.cpp"

#define LUMINANCE(r,g,b) ((75ul * (unsigned long)r + 150ul * (unsigned long)g + 29ul * (unsigned long)b)/255)

/* CRC32 computation */
static const uint32_t crc_table[16] = {
  0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
  0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
  0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
  0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};

static uint32_t crc_update(uint32_t crc, uint8_t data)
{
  uint8_t tbl_idx;
  tbl_idx = crc ^ (data >> (0 * 4));
  crc = crc_table[tbl_idx & 0x0f] ^ (crc >> 4);
  tbl_idx = crc ^ (data >> (1 * 4));
  crc = crc_table[tbl_idx & 0x0f] ^ (crc >> 4);
  return crc;
}

/* b8: Output byte n and update the CRC32 */
static void b8(struct pngout *s, uint8_t n)
{
  s->output[s->nout++] = n;
  s->crc32 = crc_update(s->crc32, n);
}

/* adler8: Output byte n and update the CRC32 and Adler CRC */
static void adler8(struct pngout *s, uint8_t n)
{
  #if USE_MINIZ > 0
    in_data[in_size++] = n;
  #else
    b8(s, n);
    s->s1 = ((uint32_t)s->s1 + n) % 65521;
    s->s2 = ((uint32_t)s->s2 + s->s1) % 65521;
  #endif
}

/* be32: Output 32-bit n big-endian */
static void be32(struct pngout *s, uint32_t n)
{
  b8(s, n >> 24);
  b8(s, n >> 16);
  b8(s, n >> 8);
  b8(s, n);
}

/* le16: Output 16-bit n little-endian */
static void le16(struct pngout *s, uint32_t n)
{
  b8(s, n);
  b8(s, n >> 8);
}

/* b8s: Output an array of bytes */
static void b8s(struct pngout *s, const uint8_t *b, size_t n)
{
  while (n--)
    b8(s, *b++);
}

/* start_chunk: output the start of a standard PNG chunk */
static void start_chunk(struct pngout *s, const char *typecode, uint32_t size)
{
  be32(s, size);
  s->crc32 = 0xffffffffUL;
  b8s(s, (const uint8_t*)typecode, 4);
}

/* end_chunk: output the CRC32 at the end of a PNG chunk */
static void end_chunk(struct pngout *s)
{
  be32(s, 0xffffffffUL ^ s->crc32);
}

void pngout_start(struct pngout *s, uint16_t width, uint16_t height, uint8_t grayscale)
{
  s->w = width;
  s->h = height;
  s->nout = 0;
  s->grayscale = grayscale;

  static uint8_t first[8] = {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
  b8s(s, first, sizeof(first));

  start_chunk(s, "IHDR", 13);
  be32(s, width);
  be32(s, height);
  const uint8_t rgb[5] = {8, grayscale ? 0 : 2, 0, 0, 0};
  b8s(s, rgb, sizeof(rgb));
  end_chunk(s);

  s->i = 0;                             /* Pixel coordinate (0,0) */
  s->j = 0;
  #if USE_MINIZ > 0
    initCompressor();
  #endif
}

/* pngout_transparent_rgb: call immediately after pngout_start to set a transparent color */
void pngout_transparent_rgb(struct pngout *s, uint8_t r, uint8_t g, uint8_t b)
{
  s->nout = 0;
  start_chunk(s, "tRNS", s->grayscale ? 2 : 6);
  if(s->grayscale) {
    b8(s, 0);
    b8(s, LUMINANCE(r,g,b));
  } else {
    b8(s, 0);
    b8(s, r);
    b8(s, 0);
    b8(s, g);
    b8(s, 0);
    b8(s, b);
  }
  end_chunk(s);
}

void pngout_utf8_text(struct pngout *s, char *keyword, char *utf8_str) {
  s->nout = 0;
  const short keywordLen = strlen(keyword);
  const short textstrLen = strlen(utf8_str);
  start_chunk(s, "iTXt", keywordLen + 5 + textstrLen);
  b8s(s, (uint8_t*)keyword, keywordLen);
  b8(s, 0); // Null separator
  b8(s, 0); // Compression flag
  b8(s, 0); // Compression method
  b8(s, 0); // Null separator
  b8(s, 0); // Null separator
  b8s(s, (uint8_t*)utf8_str, textstrLen);
  end_chunk(s);
}

void pngout_rgb(struct pngout *s, uint8_t r, uint8_t g, uint8_t b)
{
  short finished = 0;
  s->nout = 0;

  if (s->i == 0) {                      /* Start a block for each line */
    if (s->j == 0) {                    /* If 1st line, output chunk header */
      #if USE_MINIZ > 0
      #else
      s->bpl = 1 + (s->grayscale ? 1 : 3) * s->w; /* Bytes per line */
      start_chunk(s, "IDAT", 2 + s->h * (5 + s->bpl) + 4);
      b8(s, 0x78);                      /* Start DEFLATE blocks */
      b8(s, 0x01);
      s->s1 = 1;                        /* Seed the Adler CRC */
      s->s2 = 0;
      #endif
    }
    #if USE_MINIZ == 0
    b8(s, (s->j + 1) == s->h);          /* 1 if last line, 0 otherwise */
    le16(s, s->bpl);                    /* Bytes per line, and inverted */
    le16(s, ~s->bpl);
    #endif
    adler8(s, 0);                       /* Filter: none */
  }

  if(s->grayscale) {
    adler8(s, LUMINANCE(r,g,b));
  } else {
    adler8(s, r);                       /* The pixel data itself */
    adler8(s, g);
    adler8(s, b);
  }
  s->i++;

  if (s->i == s->w) {                   /* End of line means end of block */
    s->i = 0;
    if (s->j < s->h) {
      s->j++;
    }
  }
}

void pngout_end(struct pngout *s) {
  s->nout = 0;
  #if USE_MINIZ > 0
  be32(s, ((uint32_t)s->s2 << 16)   /* Append the Adler CRC */
                   + s->s1);
  end_chunk(s);                     /* End of the IDAT chunk */
  #endif
  start_chunk(s, "IEND", 0);        /* IEND chunk means end of file */
  end_chunk(s);
}

Boolean pngout_has_data(struct pngout *s, size_t *howMuch) {
  #if USE_MINIZ
    if (s->nout == 0) {
      outputCompressedData(s, s->j == s->h);
    }
  #endif

  size_t avail = s->nout;
  s->nout = 0;
  *howMuch = avail;
  return (avail > 0);
}