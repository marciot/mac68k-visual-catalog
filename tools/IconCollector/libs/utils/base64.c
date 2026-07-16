/*
 * Base64 encoding/decoding (RFC1341)
 * Copyright (c) 2005, Jouni Malinen <jkmaline@cc.hut.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#include <stdlib.h>
//#include <string.h>

#include "base64.h"

#define LINEWRAP 0

/**
 * base64_encode - Base64 encode
 * @src: Data to be encoded
 * @len: Length of the data to be encoded
 * @out_len: Pointer to output length variable, or %NULL if not used
 * Returns: Allocated buffer of out_len bytes of encoded data,
 * or %NULL on failure
 *
 * Caller is responsible for freeing the returned buffer. Returned buffer is
 * nul terminated to make it easier to use as a C string. The nul terminator is
 * not included in out_len.
 */
char * base64_encode(const unsigned char *src, size_t len,
                     char *out, size_t *out_len)
{
    const unsigned char base64_table[64] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    char *pos;
    const unsigned char *end, *in;
    size_t olen;
    #if LINEWRAP > 0
        int line_len;
    #endif

    olen = len * 4 / 3 + 4; /* 3-byte blocks to 4-byte */
    #if LINEWRAP > 0
        olen += olen / 72; /* line feeds */
    #endif
    olen++; /* nul termination */

    if (olen > *out_len) {
        return NULL;
    }

    end = src + len;
    in = src;
    pos = out;
    #if LINEWRAP
        line_len = 0;
    #endif
    while (end - in >= 3) {
        *pos++ = base64_table[in[0] >> 2];
        *pos++ = base64_table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
        *pos++ = base64_table[((in[1] & 0x0f) << 2) | (in[2] >> 6)];
        *pos++ = base64_table[in[2] & 0x3f];
        in += 3;
        #if LINEWRAP
          line_len += 4;
          if (line_len >= LINEWRAP) {
              *pos++ = '\n';
              line_len = 0;
          }
        #endif
    }

    if (end - in) {
        *pos++ = base64_table[in[0] >> 2];
        if (end - in == 1) {
            *pos++ = base64_table[(in[0] & 0x03) << 4];
            *pos++ = '=';
        } else {
            *pos++ = base64_table[((in[0] & 0x03) << 4) |
                                                  (in[1] >> 4)];
            *pos++ = base64_table[(in[1] & 0x0f) << 2];
        }
        *pos++ = '=';
        #if LINEWRAP
            line_len += 4;
        #endif
    }

    #if LINEWRAP
        if (line_len)
            *pos++ = '\n';
    #endif

    *pos = '\0';
    if (out_len)
        *out_len = pos - out;
    return out;
}

#if 0
/**
 * base64_decode - Base64 decode
 * @src: Data to be decoded
 * @len: Length of the data to be decoded
 * @out_len: Pointer to output length variable
 * Returns: Allocated buffer of out_len bytes of decoded data,
 * or %NULL on failure
 *
 * Caller is responsible for freeing the returned buffer.
 */
unsigned char * base64_decode(const unsigned char *src, size_t len,
                              size_t *out_len)
{
    const unsigned char base64_table[64] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    unsigned char dtable[256], *out, *pos, in[4], block[4], tmp;
    size_t i, count/*, olen*/;

    memset(dtable, 0x80, 256);
    for (i = 0; i < sizeof(base64_table); i++)
        dtable[base64_table[i]] = i;
    dtable['='] = 0;

    count = 0;
    for (i = 0; i < len; i++) {
        if (dtable[src[i]] != 0x80)
            count++;
    }

    if (count % 4)
        return NULL;

    /*olen = count / 4 * 3;*/
    pos = out = malloc(count);
    if (out == NULL)
        return NULL;

    count = 0;
    for (i = 0; i < len; i++) {
        tmp = dtable[src[i]];
        if (tmp == 0x80)
            continue;

        in[count] = src[i];
        block[count] = tmp;
        count++;
        if (count == 4) {
            *pos++ = (block[0] << 2) | (block[1] >> 4);
            *pos++ = (block[1] << 4) | (block[2] >> 2);
            *pos++ = (block[2] << 6) | block[3];
            count = 0;
        }
    }

    if (pos > out) {
        if (in[2] == '=')
            pos -= 2;
        else if (in[3] == '=')
            pos--;
    }

    *out_len = pos - out;
    return out;
}
#endif