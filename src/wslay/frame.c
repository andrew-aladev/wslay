/*
 * Wslay - The WebSocket Library
 *
 * Copyright (c) 2011, 2012 Tatsuhiro Tsujikawa
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <endian.h>
#include <arpa/inet.h>

#include "frame.h"

#define wslay_min(A, B) (((A) < (B)) ? (A) : (B))

extern inline
wslay_frame_context * wslay_frame_context_new ( void * ctx, const struct wslay_frame_callbacks * callbacks, void * user_data );

int16_t wslay_frame_send ( wslay_frame_context * ctx, struct wslay_frame_iocb * iocb, size_t * length )
{
    if ( iocb->data_length > iocb->payload_length ) {
        return WSLAY_ERR_INVALID_ARGUMENT;
    }
    if ( ctx->ostate == PREP_HEADER ) {
        uint8_t *hdptr = ctx->oheader;
        memset ( ctx->oheader, 0, sizeof ( ctx->oheader ) );
        * hdptr |= ( iocb->fin << 7 ) & 0x80u;
        * hdptr |= ( iocb->rsv << 4 ) & 0x70u;
        * hdptr |= iocb->opcode & 0xfu;
        hdptr ++;
        * hdptr |= ( iocb->mask << 7 ) & 0x80u;
        if ( wslay_is_ctrl_frame ( iocb->opcode ) && iocb->payload_length > 125 ) {
            return WSLAY_ERR_INVALID_ARGUMENT;
        }
        if ( iocb->payload_length < 126 ) {
            * hdptr |= iocb->payload_length;
            hdptr ++;
        } else if ( iocb->payload_length < ( 1 << 16 ) ) {
            uint16_t len = htons ( iocb->payload_length );
            * hdptr |= 126;
            hdptr ++;
            memcpy ( hdptr, &len, 2 );
            hdptr += 2;
        } else if ( iocb->payload_length < ( 1ull << 63 ) ) {
            uint64_t len = htobe64 ( iocb->payload_length );
            * hdptr |= 127;
            hdptr ++;
            memcpy ( hdptr, &len, 8 );
            hdptr += 8;
        } else {
            // Too large payload length
            return WSLAY_ERR_INVALID_ARGUMENT;
        }
        if ( iocb->mask ) {
            if ( ctx->callbacks.genmask_callback ( ctx->omaskkey, 4, ctx->user_data ) != 0 ) {
                return WSLAY_ERR_INVALID_CALLBACK;
            } else {
                ctx->omask = 1;
                memcpy ( hdptr, ctx->omaskkey, 4 );
                hdptr += 4;
            }
        }
        ctx->ostate = SEND_HEADER;
        ctx->oheadermark = ctx->oheader;
        ctx->oheaderlimit = hdptr;
        ctx->opayloadlen = iocb->payload_length;
        ctx->opayloadoff = 0;
    }
    if ( ctx->ostate == SEND_HEADER ) {
        ptrdiff_t len = ctx->oheaderlimit - ctx->oheadermark;
        ssize_t r;
        int flags = 0;
        if ( iocb->data_length > 0 ) {
            flags |= WSLAY_MSG_MORE;
        };
        r = ctx->callbacks.send_callback ( ctx->oheadermark, len, flags, ctx->user_data, false );
        if ( r > 0 ) {
            if ( r > len ) {
                return WSLAY_ERR_INVALID_CALLBACK;
            } else {
                ctx->oheadermark += r;
                if ( ctx->oheadermark == ctx->oheaderlimit ) {
                    ctx->ostate = SEND_PAYLOAD;
                } else {
                    return WSLAY_ERR_WANT_WRITE;
                }
            }
        } else {
            return WSLAY_ERR_WANT_WRITE;
        }
    }
    if ( ctx->ostate == SEND_PAYLOAD ) {
        size_t totallen = 0;
        if ( iocb->data_length > 0 ) {
            if ( ctx->omask ) {
                uint8_t temp[4096];
                const uint8_t * datamark  = iocb->data;
                const uint8_t * datalimit = datamark + iocb->data_length;
                while ( datamark < datalimit ) {
                    size_t datalen = datalimit - datamark;
                    const uint8_t *writelimit = datamark + wslay_min ( sizeof ( temp ), datalen );
                    size_t writelen = writelimit - datamark;
                    ssize_t r;
                    size_t i;
                    for ( i = 0; i < writelen; ++i ) {
                        temp[i] = datamark[i] ^ ctx->omaskkey[ ( ctx->opayloadoff + i ) % 4];
                    }
                    r = ctx->callbacks.send_callback ( temp, writelen, 0, ctx->user_data, false );
                    if ( r > 0 ) {
                        if ( ( size_t ) r > writelen ) {
                            return WSLAY_ERR_INVALID_CALLBACK;
                        } else {
                            datamark += r;
                            ctx->opayloadoff += r;
                            totallen += r;
                        }
                    } else {
                        if ( totallen > 0 ) {
                            break;
                        } else {
                            return WSLAY_ERR_WANT_WRITE;
                        }
                    }
                }
            } else {
                ssize_t r;
                r = ctx->callbacks.send_callback ( iocb->data, iocb->data_length, 0, ctx->user_data, true );
                if ( r > 0 ) {
                    if ( ( size_t ) r > iocb->data_length ) {
                        return WSLAY_ERR_INVALID_CALLBACK;
                    } else {
                        ctx->opayloadoff += r;
                        totallen = r;
                    }
                } else {
                    return WSLAY_ERR_WANT_WRITE;
                }
            }
        }
        if ( ctx->opayloadoff == ctx->opayloadlen ) {
            ctx->ostate = PREP_HEADER;
        }
        * length = totallen;
        return 0;
    }
    return WSLAY_ERR_INVALID_ARGUMENT;
}

static inline
void wslay_shift_ibuf ( wslay_frame_context * ctx )
{
    ptrdiff_t len = ctx->ibuflimit - ctx->ibufmark;
    memmove ( ctx->ibuf, ctx->ibufmark, len );
    ctx->ibuflimit = ctx->ibuf + len;
    ctx->ibufmark = ctx->ibuf;
}

static inline
int16_t wslay_recv ( wslay_frame_context * ctx )
{
    if ( ctx->ibufmark != ctx->ibuf ) {
        wslay_shift_ibuf ( ctx );
    }
    ssize_t result;
    result = ctx->callbacks.recv_callback ( ctx->ibuflimit, ctx->ibuf + sizeof ( ctx->ibuf ) - ctx->ibuflimit, 0, ctx->user_data );
    if ( result > 0 ) {
        ctx->ibuflimit += result;
    } else {
        return WSLAY_ERR_WANT_READ;
    }
    return 0;
}

#define WSLAY_AVAIL_IBUF(ctx) ((size_t)(ctx->ibuflimit - ctx->ibufmark))

int16_t wslay_frame_recv ( wslay_frame_context * ctx, struct wslay_frame_iocb * iocb, size_t * data_length_ptr )
{
    int16_t result;
    if ( ctx->istate == RECV_HEADER1 ) {
        uint8_t fin, opcode, rsv, payloadlen;
        if ( WSLAY_AVAIL_IBUF ( ctx ) < ctx->ireqread ) {
            if ( ( result = wslay_recv ( ctx ) ) < 0 ) {
                return result;
            }
        }
        if ( WSLAY_AVAIL_IBUF ( ctx ) < ctx->ireqread ) {
            return WSLAY_ERR_WANT_READ;
        }
        fin = ( ctx->ibufmark[0] >> 7 ) & 1;
        rsv = ( ctx->ibufmark[0] >> 4 ) & 7;
        opcode = ctx->ibufmark[0] & 0xfu;
        ctx->iom.opcode = opcode;
        ctx->iom.fin = fin;
        ctx->iom.rsv = rsv;
        ++ctx->ibufmark;
        ctx->imask = ( ctx->ibufmark[0] >> 7 ) & 1;
        payloadlen = ctx->ibufmark[0] & 0x7fu;
        ++ctx->ibufmark;
        if ( wslay_is_ctrl_frame ( opcode ) && ( payloadlen > 125 || !fin ) ) {
            return WSLAY_ERR_PROTO;
        }
        if ( payloadlen == 126 ) {
            ctx->istate = RECV_EXT_PAYLOADLEN;
            ctx->ireqread = 2;
        } else if ( payloadlen == 127 ) {
            ctx->istate = RECV_EXT_PAYLOADLEN;
            ctx->ireqread = 8;
        } else {
            ctx->ipayloadlen = payloadlen;
            ctx->ipayloadoff = 0;
            if ( ctx->imask ) {
                ctx->istate = RECV_MASKKEY;
                ctx->ireqread = 4;
            } else {
                ctx->istate = RECV_PAYLOAD;
            }
        }
    }

    if ( ctx->istate == RECV_EXT_PAYLOADLEN ) {
        if ( WSLAY_AVAIL_IBUF ( ctx ) < ctx->ireqread ) {
            if ( ( result = wslay_recv ( ctx ) ) < 0 ) {
                return result;
            }
            if ( WSLAY_AVAIL_IBUF ( ctx ) < ctx->ireqread ) {
                return WSLAY_ERR_WANT_READ;
            }
        }
        ctx->ipayloadlen = 0;
        ctx->ipayloadoff = 0;
        memcpy ( ( uint8_t* ) &ctx->ipayloadlen + ( 8 - ctx->ireqread ), ctx->ibufmark, ctx->ireqread );
        ctx->ipayloadlen = be64toh ( ctx->ipayloadlen );
        ctx->ibufmark += ctx->ireqread;
        if ( ctx->ireqread == 8 ) {
            if ( ctx->ipayloadlen < ( 1 << 16 ) || ctx->ipayloadlen & ( 1ull << 63 ) ) {
                return WSLAY_ERR_PROTO;
            }
        } else if ( ctx->ipayloadlen < 126 ) {
            return WSLAY_ERR_PROTO;
        }
        if ( ctx->imask ) {
            ctx->istate = RECV_MASKKEY;
            ctx->ireqread = 4;
        } else {
            ctx->istate = RECV_PAYLOAD;
        }
    }

    if ( ctx->istate == RECV_MASKKEY ) {
        if ( WSLAY_AVAIL_IBUF ( ctx ) < ctx->ireqread ) {
            if ( ( result = wslay_recv ( ctx ) ) < 0 ) {
                return result;
            }
            if ( WSLAY_AVAIL_IBUF ( ctx ) < ctx->ireqread ) {
                return WSLAY_ERR_WANT_READ;
            }
        }
        memcpy ( ctx->imaskkey, ctx->ibufmark, 4 );
        ctx->ibufmark += 4;
        ctx->istate = RECV_PAYLOAD;
    }

    if ( ctx->istate == RECV_PAYLOAD ) {
        uint8_t *readlimit, *readmark;
        uint64_t rempayloadlen = ctx->ipayloadlen - ctx->ipayloadoff;
        if ( WSLAY_AVAIL_IBUF ( ctx ) == 0 && rempayloadlen > 0 ) {
            if ( ( result = wslay_recv ( ctx ) ) < 0 ) {
                return result;
            }
        }
        readmark = ctx->ibufmark;
        if ( WSLAY_AVAIL_IBUF ( ctx ) < rempayloadlen ) {
            readlimit = ctx->ibuflimit;
        } else {
            readlimit = ctx->ibufmark + rempayloadlen;
        }
        if ( ctx->imask ) {
            for ( ; ctx->ibufmark != readlimit; ++ctx->ibufmark, ++ctx->ipayloadoff ) {
                ctx->ibufmark[0] ^= ctx->imaskkey[ctx->ipayloadoff % 4];
            }
        } else {
            ctx->ibufmark = readlimit;
            ctx->ipayloadoff += readlimit - readmark;
        }

        iocb->fin            = ctx->iom.fin;
        iocb->rsv            = ctx->iom.rsv;
        iocb->opcode         = ctx->iom.opcode;
        iocb->payload_length = ctx->ipayloadlen;
        iocb->mask           = ctx->imask;
        iocb->data           = readmark;
        iocb->data_length    = ctx->ibufmark - readmark;

        if ( ctx->ipayloadlen == ctx->ipayloadoff ) {
            ctx->istate = RECV_HEADER1;
            ctx->ireqread = 2;
        }

        * data_length_ptr = iocb->data_length;
        return 0;
    }
    return WSLAY_ERR_INVALID_ARGUMENT;
}
