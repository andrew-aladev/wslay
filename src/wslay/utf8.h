/* Copyright (c) 2008-2010 Bjoern Hoehrmann <bjoern@hoehrmann.de>
 * See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.
 *
 * Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef WSLAY_UTF8_H
#define WSLAY_UTF8_H

#include <stdint.h>

#define UTF8_ACCEPT 0
#define UTF8_REJECT 12

extern
const uint8_t wslay_utf8d[];

inline
uint32_t decode ( uint32_t * state_ptr, uint32_t * codep_ptr, uint32_t byte )
{
    uint32_t state = * state_ptr;
    uint32_t codep = * codep_ptr;
    uint32_t type  = wslay_utf8d[byte];

    if ( state != UTF8_ACCEPT ) {
        * codep_ptr = ( byte & 0x3fu ) | ( codep << 6 );
    } else {
        * codep_ptr = ( 0xff >> type ) & byte;
    }

    * state_ptr = state = wslay_utf8d[256 + state + type];
    return state;
}

#endif