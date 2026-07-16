// https://www.unicode.org/Public/MAPPINGS/VENDORS/APPLE/ROMAN.TXT

#include "stdint.h"

typedef struct {
  unsigned char  binary;
  unsigned short unicode;
} UnicodeMap[];

typedef uint16_t UnicodeLookup[256];

extern UnicodeLookup macRoman;

void initUnicodeTables();

char *toUTF8Char(UnicodeLookup table, const char *src, char *dst);
char *toFilename(UnicodeLookup table, const char *src, char *dst, char escape);
