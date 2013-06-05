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

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <arpa/inet.h>

#include "event.h"
#include "queue.h"
#include "frame.h"

#include <talloc2/tree.h>

static inline
void wslay_event_imsg_set ( struct wslay_event_imsg * m, uint8_t fin, uint8_t rsv, uint8_t opcode )
{
    m->fin = fin;
    m->rsv = rsv;
    m->opcode = opcode;
    m->msg_length = 0;
}

extern inline
void wslay_event_imsg_reset ( struct wslay_event_imsg * m );

static inline
uint8_t wslay_event_imsg_append_chunk ( struct wslay_event_imsg * m, size_t len )
{
    if ( len == 0 ) {
        return 0;
    }
    struct wslay_event_byte_chunk * chunk = talloc ( m->chunks, sizeof ( struct wslay_event_byte_chunk ) );
    if ( chunk == NULL ) {
        return 1;
    }
    if ( len != 0 ) {
        void * data = talloc ( chunk, len * sizeof ( uint8_t ) );
        if ( data == NULL ) {
            talloc_free ( chunk );
            return 2;
        }
        chunk->data        = data;
        chunk->data_length = len;
    } else {
        chunk->data        = NULL;
        chunk->data_length = 0;
    }

    if ( wslay_queue_push ( m->chunks, chunk ) != 0 ) {
        return 2;
    }
    m->msg_length += len;
    return 0;
}

static inline
struct wslay_event_omsg * wslay_event_omsg_non_fragmented_new ( void * ctx, uint8_t opcode, const uint8_t * msg, size_t msg_length )
{
    struct wslay_event_omsg * omsg = talloc ( ctx, sizeof ( struct wslay_event_omsg ) );
    if ( omsg == NULL ) {
        return NULL;
    }
    omsg->fin    = 1;
    omsg->opcode = opcode;
    omsg->type   = WSLAY_NON_FRAGMENTED;

    if ( msg_length != 0 ) {
        void * data = talloc ( omsg, msg_length );
        if ( data == NULL ) {
            talloc_free ( omsg );
            return NULL;
        }
        memcpy ( data, msg, msg_length );

        omsg->data        = data;
        omsg->data_length = msg_length;
    } else {
        omsg->data        = NULL;
        omsg->data_length = 0;
    }
    omsg->read_callback = NULL;
    memset ( &omsg->source, 0, sizeof ( omsg->source ) );

    return omsg;
}

static inline
struct wslay_event_omsg * wslay_event_omsg_fragmented_new ( void * ctx, uint8_t opcode, const union wslay_event_msg_source source, wslay_event_fragmented_msg_callback read_callback )
{
    struct wslay_event_omsg * omsg = talloc ( ctx, sizeof ( struct wslay_event_omsg ) );
    if ( omsg == NULL ) {
        return NULL;
    }
    omsg->fin    = 0;
    omsg->opcode = opcode;
    omsg->type   = WSLAY_FRAGMENTED;

    omsg->data        = NULL;
    omsg->data_length = 0;

    omsg->source        = source;
    omsg->read_callback = read_callback;

    return omsg;
}

static inline
uint8_t * wslay_event_flatten_queue ( wslay_queue * queue, size_t len )
{
    if ( len == 0 ) {
        return NULL;
    } else {
        size_t off    = 0;
        uint8_t * buf = talloc ( queue, len * sizeof ( uint8_t ) );
        if ( buf == NULL ) {
            return NULL;
        }

        while ( !wslay_queue_is_empty ( queue ) ) {
            struct wslay_event_byte_chunk * chunk = wslay_queue_top ( queue );
            memcpy ( buf + off, chunk->data, chunk->data_length );
            off += chunk->data_length;
            talloc_free ( chunk );
            if ( wslay_queue_pop ( queue ) != 0 ) {
                return NULL;
            }
            if ( off > len ) {
                return NULL;
            }
        }

        if ( len != off ) {
            return NULL;
        }
        return buf;
    }
}

static inline
bool wslay_event_is_msg_queueable ( wslay_event_context * ctx )
{
    return ctx->write_enabled && ( ctx->close_status & WSLAY_CLOSE_QUEUED ) == 0;
}

int wslay_event_queue_close ( wslay_event_context * ctx, uint16_t status_code, const uint8_t *reason, size_t reason_length )
{
    if ( !wslay_event_is_msg_queueable ( ctx ) ) {
        return 1;
    } else if ( reason_length > 123 ) {
        return 2;
    }

    uint8_t msg[128];
    size_t msg_length;
    struct wslay_event_msg arg;
    uint16_t ncode;
    int r;
    if ( status_code == 0 ) {
        msg_length = 0;
    } else {
        ncode = htons ( status_code );
        memcpy ( msg, &ncode, 2 );
        memcpy ( msg + 2, reason, reason_length );
        msg_length = reason_length + 2;
    }
    arg.opcode     = WSLAY_CONNECTION_CLOSE;
    arg.msg        = msg;
    arg.msg_length = msg_length;
    r = wslay_event_queue_msg ( ctx, &arg );
    if ( r == 0 ) {
        ctx->close_status |= WSLAY_CLOSE_QUEUED;
    }
    return r;
}

static int wslay_event_queue_close_wrapper ( wslay_event_context * ctx, uint16_t status_code, const uint8_t *reason, size_t reason_length )
{
    int r;
    ctx->read_enabled = 0;
    if ( ( r = wslay_event_queue_close ( ctx, status_code, reason, reason_length ) ) &&
            r != WSLAY_ERR_NO_MORE_MSG ) {
        return r;
    }
    return 0;
}

int wslay_event_queue_msg ( wslay_event_context * ctx, const struct wslay_event_msg * arg )
{
    int r;
    if ( !wslay_event_is_msg_queueable ( ctx ) ) {
        return WSLAY_ERR_NO_MORE_MSG;
    }
    if ( wslay_is_ctrl_frame ( arg->opcode ) && arg->msg_length > 125 ) {
        return WSLAY_ERR_INVALID_ARGUMENT;
    }

    wslay_queue * queue;
    if ( wslay_is_ctrl_frame ( arg->opcode ) ) {
        queue = ctx->send_ctrl_queue;
    } else {
        queue = ctx->send_queue;
    }

    struct wslay_event_omsg * omsg = wslay_event_omsg_non_fragmented_new ( queue, arg->opcode, arg->msg, arg->msg_length );
    if ( omsg == NULL ) {
        return -1;
    }
    if ( ( r = wslay_queue_push ( queue, omsg ) ) != 0 ) {
        return r;
    }

    ctx->queued_msg_count++;
    ctx->queued_msg_length += arg->msg_length;
    return 0;
}

int wslay_event_queue_fragmented_msg ( wslay_event_context * ctx, const struct wslay_event_fragmented_msg *arg )
{
    if ( !wslay_event_is_msg_queueable ( ctx ) ) {
        return WSLAY_ERR_NO_MORE_MSG;
    }
    if ( wslay_is_ctrl_frame ( arg->opcode ) ) {
        return WSLAY_ERR_INVALID_ARGUMENT;
    }
    struct wslay_event_omsg * omsg = wslay_event_omsg_fragmented_new ( ctx->send_queue, arg->opcode, arg->source, arg->read_callback );
    if ( omsg == NULL ) {
        return -1;
    }
    if ( wslay_queue_push ( ctx->send_queue, omsg ) != 0 ) {
        return -1;
    }
    ctx->queued_msg_count ++;
    return 0;
}

static void wslay_event_call_on_frame_recv_start_callback ( wslay_event_context * ctx, const struct wslay_frame_iocb *iocb )
{
    if ( ctx->callbacks.on_frame_recv_start_callback ) {
        struct wslay_event_on_frame_recv_start_arg arg;
        arg.fin = iocb->fin;
        arg.rsv = iocb->rsv;
        arg.opcode = iocb->opcode;
        arg.payload_length = iocb->payload_length;
        ctx->callbacks.on_frame_recv_start_callback ( ctx, &arg, ctx->user_data );
    }
}

static void wslay_event_call_on_frame_recv_chunk_callback ( wslay_event_context * ctx, const struct wslay_frame_iocb *iocb )
{
    if ( ctx->callbacks.on_frame_recv_chunk_callback ) {
        struct wslay_event_on_frame_recv_chunk_arg arg;
        arg.data = iocb->data;
        arg.data_length = iocb->data_length;
        ctx->callbacks.on_frame_recv_chunk_callback ( ctx, &arg, ctx->user_data );
    }
}

static void wslay_event_call_on_frame_recv_end_callback ( wslay_event_context * ctx )
{
    if ( ctx->callbacks.on_frame_recv_end_callback ) {
        ctx->callbacks.on_frame_recv_end_callback ( ctx, ctx->user_data );
    }
}

static int wslay_event_is_valid_status_code ( uint16_t status_code )
{
    return ( 1000 <= status_code && status_code <= 1011 &&
             status_code != 1004 && status_code != 1005 && status_code != 1006 ) ||
           ( 3000 <= status_code && status_code <= 4999 );
}

static int wslay_event_config_get_no_buffering ( wslay_event_context * ctx )
{
    return ( ctx->config & WSLAY_CONFIG_NO_BUFFERING ) > 0;
}

int wslay_event_recv ( wslay_event_context * ctx )
{
    struct wslay_frame_iocb iocb;
    ssize_t r;
    size_t data_length;
    int16_t result;
    while ( ctx->read_enabled ) {
        memset ( &iocb, 0, sizeof ( iocb ) );
        if ( ( result = wslay_frame_recv ( ctx->frame_ctx, &iocb, &data_length ) ) == 0 ) {
            int new_frame = 0;
            /* We only allow rsv == 0 ATM. */
            if ( iocb.rsv != 0 || ( ( ctx->server && !iocb.mask ) || ( !ctx->server && iocb.mask ) ) ) {
                if ( ( r = wslay_event_queue_close_wrapper ( ctx, WSLAY_CODE_PROTOCOL_ERROR, NULL, 0 ) ) != 0 ) {
                    return r;
                }
                break;
            }
            if ( ctx->imsg->opcode == 0xffu ) {
                if (
                    iocb.opcode == WSLAY_TEXT_FRAME ||
                    iocb.opcode == WSLAY_BINARY_FRAME ||
                    iocb.opcode == WSLAY_CONNECTION_CLOSE ||
                    iocb.opcode == WSLAY_PING ||
                    iocb.opcode == WSLAY_PONG
                ) {
                    wslay_event_imsg_set ( ctx->imsg, iocb.fin, iocb.rsv, iocb.opcode );
                    new_frame = 1;
                } else {
                    if ( ( r = wslay_event_queue_close_wrapper ( ctx, WSLAY_CODE_PROTOCOL_ERROR, NULL, 0 ) ) != 0 ) {
                        return r;
                    }
                    break;
                }
            } else if ( ctx->ipayloadlen == 0 && ctx->ipayloadoff == 0 ) {
                if ( iocb.opcode == WSLAY_CONTINUATION_FRAME ) {
                    ctx->imsg->fin = iocb.fin;
                } else if (
                    iocb.opcode == WSLAY_CONNECTION_CLOSE ||
                    iocb.opcode == WSLAY_PING ||
                    iocb.opcode == WSLAY_PONG
                ) {
                    ctx->imsg = &ctx->imsgs[1];
                    wslay_event_imsg_set ( ctx->imsg, iocb.fin, iocb.rsv, iocb.opcode );
                } else {
                    if ( ( r = wslay_event_queue_close_wrapper ( ctx, WSLAY_CODE_PROTOCOL_ERROR, NULL, 0 ) ) != 0 ) {
                        return r;
                    }
                    break;
                }
                new_frame = 1;
            }
            if ( new_frame ) {
                if ( ctx->imsg->msg_length + iocb.payload_length > ctx->max_recv_msg_length ) {
                    if ( ( r = wslay_event_queue_close_wrapper ( ctx, WSLAY_CODE_MESSAGE_TOO_BIG, NULL, 0 ) ) != 0 ) {
                        return r;
                    }
                    break;
                }
                ctx->ipayloadlen = iocb.payload_length;
                wslay_event_call_on_frame_recv_start_callback ( ctx, &iocb );
                if ( !wslay_event_config_get_no_buffering ( ctx ) || wslay_is_ctrl_frame ( iocb.opcode ) ) {
                    if ( wslay_event_imsg_append_chunk ( ctx->imsg, iocb.payload_length ) != 0 ) {
                        ctx->read_enabled = 0;
                        return -1;
                    }
                }
            }
            if ( ctx->imsg->opcode == WSLAY_TEXT_FRAME || ctx->imsg->opcode == WSLAY_CONNECTION_CLOSE ) {
                size_t i;
                if ( ctx->imsg->opcode == WSLAY_CONNECTION_CLOSE ) {
                    i = 2;
                } else {
                    i = 0;
                }
                for ( ; i < iocb.data_length; ++i ) {
                    uint32_t codep;
                    if ( decode ( &ctx->imsg->utf8state, &codep, iocb.data[i] ) == UTF8_REJECT ) {
                        if ( ( r = wslay_event_queue_close_wrapper ( ctx, WSLAY_CODE_INVALID_FRAME_PAYLOAD_DATA, NULL, 0 ) ) != 0 ) {
                            return r;
                        }
                        break;
                    }
                }
            }
            if ( ctx->imsg->utf8state == UTF8_REJECT ) {
                break;
            }
            wslay_event_call_on_frame_recv_chunk_callback ( ctx, &iocb );
            if ( iocb.data_length > 0 ) {
                if ( !wslay_event_config_get_no_buffering ( ctx ) || wslay_is_ctrl_frame ( iocb.opcode ) ) {
                    struct wslay_event_byte_chunk *chunk;
                    chunk = wslay_queue_tail ( ctx->imsg->chunks );
                    memcpy ( chunk->data + ctx->ipayloadoff, iocb.data, iocb.data_length );
                }
                ctx->ipayloadoff += iocb.data_length;
            }
            if ( ctx->ipayloadoff == ctx->ipayloadlen ) {
                if (
                    ctx->imsg->fin &&
                    ( ctx->imsg->opcode == WSLAY_TEXT_FRAME || ctx->imsg->opcode == WSLAY_CONNECTION_CLOSE ) &&
                    ctx->imsg->utf8state != UTF8_ACCEPT
                ) {
                    if ( ( r = wslay_event_queue_close_wrapper ( ctx, WSLAY_CODE_INVALID_FRAME_PAYLOAD_DATA, NULL, 0 ) ) != 0 ) {
                        return r;
                    }
                    break;
                }
                wslay_event_call_on_frame_recv_end_callback ( ctx );
                if ( ctx->imsg->fin ) {
                    if ( ctx->callbacks.on_msg_recv_callback || ctx->imsg->opcode == WSLAY_CONNECTION_CLOSE || ctx->imsg->opcode == WSLAY_PING ) {
                        struct wslay_event_on_msg_recv_arg arg;
                        uint16_t status_code = 0;
                        uint8_t *msg = NULL;
                        size_t msg_length = 0;
                        if ( !wslay_event_config_get_no_buffering ( ctx ) || wslay_is_ctrl_frame ( iocb.opcode ) ) {
                            msg = wslay_event_flatten_queue ( ctx->imsg->chunks, ctx->imsg->msg_length );
                            if ( ctx->imsg->msg_length && !msg ) {
                                ctx->read_enabled = 0;
                                return WSLAY_ERR_NOMEM;
                            }
                            msg_length = ctx->imsg->msg_length;
                        }
                        if ( ctx->imsg->opcode == WSLAY_CONNECTION_CLOSE ) {
                            const uint8_t *reason;
                            size_t reason_length;
                            if ( ctx->imsg->msg_length >= 2 ) {
                                memcpy ( &status_code, msg, 2 );
                                status_code = ntohs ( status_code );
                                if ( !wslay_event_is_valid_status_code ( status_code ) ) {
                                    talloc_free ( msg );
                                    if ( ( r = wslay_event_queue_close_wrapper ( ctx, WSLAY_CODE_PROTOCOL_ERROR, NULL, 0 ) ) != 0 ) {
                                        return r;
                                    }
                                    break;
                                }
                                reason = msg + 2;
                                reason_length = ctx->imsg->msg_length - 2;
                            } else {
                                reason = NULL;
                                reason_length = 0;
                            }
                            ctx->close_status |= WSLAY_CLOSE_RECEIVED;
                            if ( status_code == 0 ) {
                                ctx->status_code_recv = WSLAY_CODE_NO_STATUS_RCVD;
                            } else {
                                ctx->status_code_recv = status_code;
                            }
                            if ( ( r = wslay_event_queue_close_wrapper ( ctx, status_code, reason, reason_length ) ) != 0 ) {
                                talloc_free ( msg );
                                return r;
                            }
                        } else if ( ctx->imsg->opcode == WSLAY_PING ) {
                            struct wslay_event_msg arg;
                            arg.opcode = WSLAY_PONG;
                            arg.msg = msg;
                            arg.msg_length = ctx->imsg->msg_length;
                            if ( ( r = wslay_event_queue_msg ( ctx, &arg ) ) &&
                                    r != WSLAY_ERR_NO_MORE_MSG ) {
                                ctx->read_enabled = 0;
                                talloc_free ( msg );
                                return r;
                            }
                        }
                        if ( ctx->callbacks.on_msg_recv_callback ) {
                            arg.rsv = ctx->imsg->rsv;
                            arg.opcode = ctx->imsg->opcode;
                            arg.msg = msg;
                            arg.msg_length = msg_length;
                            arg.status_code = status_code;
                            ctx->error = 0;
                            ctx->callbacks.on_msg_recv_callback ( ctx, &arg, ctx->user_data );
                        }
                        talloc_free ( msg );
                    }
                    wslay_event_imsg_reset ( ctx->imsg );
                    if ( ctx->imsg == &ctx->imsgs[1] ) {
                        ctx->imsg = &ctx->imsgs[0];
                    }
                }
                ctx->ipayloadlen = ctx->ipayloadoff = 0;
            }
        } else {
            if ( result != WSLAY_ERR_WANT_READ || ( ctx->error != WSLAY_ERR_WOULDBLOCK && ctx->error != 0 ) ) {
                if ( ( r = wslay_event_queue_close_wrapper ( ctx, 0, NULL, 0 ) ) != 0 ) {
                    return r;
                }
                return WSLAY_ERR_CALLBACK_FAILURE;
            }
            break;
        }
    }
    return 0;
}

static void wslay_event_on_non_fragmented_msg_popped ( wslay_event_context * ctx )
{
    ctx->omsg->fin = 1;
    ctx->opayloadlen = ctx->omsg->data_length;
    ctx->opayloadoff = 0;
}

static struct wslay_event_omsg* wslay_event_send_ctrl_queue_pop ( wslay_event_context * ctx )
{
    /*
     * If Close control frame is queued, we don't send any control frame
     * other than Close.
     */
    if ( ctx->close_status & WSLAY_CLOSE_QUEUED ) {
        while ( !wslay_queue_is_empty ( ctx->send_ctrl_queue ) ) {
            struct wslay_event_omsg *msg = wslay_queue_top ( ctx->send_ctrl_queue );
            wslay_queue_pop ( ctx->send_ctrl_queue );
            if ( msg->opcode == WSLAY_CONNECTION_CLOSE ) {
                return msg;
            } else {
                talloc_free ( msg );
            }
        }
        return NULL;
    } else {
        struct wslay_event_omsg *msg = wslay_queue_top ( ctx->send_ctrl_queue );
        wslay_queue_pop ( ctx->send_ctrl_queue );
        return msg;
    }
}

int wslay_event_send ( wslay_event_context * ctx )
{
    struct wslay_frame_iocb iocb;
    ssize_t r;
    while ( ctx->write_enabled &&
            ( !wslay_queue_is_empty ( ctx->send_queue ) ||
              !wslay_queue_is_empty ( ctx->send_ctrl_queue ) || ctx->omsg ) ) {
        if ( !ctx->omsg ) {
            if ( wslay_queue_is_empty ( ctx->send_ctrl_queue ) ) {
                ctx->omsg = wslay_queue_top ( ctx->send_queue );
                wslay_queue_pop ( ctx->send_queue );
            } else {
                ctx->omsg = wslay_event_send_ctrl_queue_pop ( ctx );
                if ( ctx->omsg == NULL ) {
                    break;
                }
            }
            if ( ctx->omsg->type == WSLAY_NON_FRAGMENTED ) {
                wslay_event_on_non_fragmented_msg_popped ( ctx );
            }
        } else if ( !wslay_is_ctrl_frame ( ctx->omsg->opcode ) &&
                    ctx->frame_ctx->ostate == PREP_HEADER &&
                    !wslay_queue_is_empty ( ctx->send_ctrl_queue ) ) {
            if ( ( r = wslay_queue_push_front ( ctx->send_queue, ctx->omsg ) ) != 0 ) {
                ctx->write_enabled = 0;
                return r;
            }
            ctx->omsg = wslay_event_send_ctrl_queue_pop ( ctx );
            if ( ctx->omsg == NULL ) {
                break;
            }
            /* ctrl message has WSLAY_NON_FRAGMENTED */
            wslay_event_on_non_fragmented_msg_popped ( ctx );
        }
        if ( ctx->omsg->type == WSLAY_NON_FRAGMENTED ) {
            memset ( &iocb, 0, sizeof ( iocb ) );
            iocb.fin = 1;
            iocb.opcode = ctx->omsg->opcode;
            iocb.mask = !ctx->server;
            iocb.data = ctx->omsg->data + ctx->opayloadoff;
            iocb.data_length = ctx->opayloadlen - ctx->opayloadoff;
            iocb.payload_length = ctx->opayloadlen;
            size_t length;
            int16_t result = wslay_frame_send ( ctx->frame_ctx, &iocb, &length );
            if ( result == 0 ) {
                ctx->opayloadoff += length;
                if ( ctx->opayloadoff == ctx->opayloadlen ) {
                    ctx->queued_msg_count --;
                    ctx->queued_msg_length -= ctx->omsg->data_length;
                    if ( ctx->omsg->opcode == WSLAY_CONNECTION_CLOSE ) {
                        uint16_t status_code = 0;
                        ctx->write_enabled = 0;
                        ctx->close_status |= WSLAY_CLOSE_SENT;
                        if ( ctx->omsg->data_length >= 2 ) {
                            memcpy ( &status_code, ctx->omsg->data, 2 );
                            status_code = ntohs ( status_code );
                        }
                        ctx->status_code_sent =
                            status_code == 0 ? WSLAY_CODE_NO_STATUS_RCVD : status_code;
                    }
                    talloc_free ( ctx->omsg );
                    ctx->omsg = NULL;
                } else {
                    break;
                }
            } else {
                if ( result != WSLAY_ERR_WANT_WRITE || ( ctx->error != WSLAY_ERR_WOULDBLOCK && ctx->error != 0 ) ) {
                    ctx->write_enabled = 0;
                    return WSLAY_ERR_CALLBACK_FAILURE;
                }
                break;
            }
        } else {
            if ( ctx->omsg->fin == 0 && ctx->obuflimit == ctx->obufmark ) {
                int eof = 0;
                r = ctx->omsg->read_callback ( ctx, ctx->obuf, sizeof ( ctx->obuf ),
                                               &ctx->omsg->source,
                                               &eof, ctx->user_data );
                if ( r == 0 ) {
                    break;
                } else if ( r < 0 ) {
                    ctx->write_enabled = 0;
                    return WSLAY_ERR_CALLBACK_FAILURE;
                }
                ctx->obuflimit = ctx->obuf + r;
                if ( eof ) {
                    ctx->omsg->fin = 1;
                }
                ctx->opayloadlen = r;
                ctx->opayloadoff = 0;
            }
            memset ( &iocb, 0, sizeof ( iocb ) );
            iocb.fin = ctx->omsg->fin;
            iocb.opcode = ctx->omsg->opcode;
            iocb.mask = !ctx->server;
            iocb.data = ctx->obufmark;
            iocb.data_length = ctx->obuflimit - ctx->obufmark;
            iocb.payload_length = ctx->opayloadlen;
            size_t length;
            int16_t result = wslay_frame_send ( ctx->frame_ctx, &iocb, &length );
            if ( result >= 0 ) {
                ctx->obufmark += length;
                if ( ctx->obufmark == ctx->obuflimit ) {
                    ctx->obufmark = ctx->obuflimit = ctx->obuf;
                    if ( ctx->omsg->fin ) {
                        ctx->queued_msg_count --;
                        talloc_free ( ctx->omsg );
                        ctx->omsg = NULL;
                    } else {
                        ctx->omsg->opcode = WSLAY_CONTINUATION_FRAME;
                    }
                } else {
                    break;
                }
            } else {
                if ( result != WSLAY_ERR_WANT_WRITE || ( ctx->error != WSLAY_ERR_WOULDBLOCK && ctx->error != 0 ) ) {
                    ctx->write_enabled = 0;
                    return WSLAY_ERR_CALLBACK_FAILURE;
                }
                break;
            }
        }
    }
    return 0;
}

void wslay_event_set_error ( wslay_event_context * ctx, int val )
{
    ctx->error = val;
}

int wslay_event_want_read ( wslay_event_context * ctx )
{
    return ctx->read_enabled;
}

int wslay_event_want_write ( wslay_event_context * ctx )
{
    return ctx->write_enabled &&
           ( !wslay_queue_is_empty ( ctx->send_queue ) ||
             !wslay_queue_is_empty ( ctx->send_ctrl_queue ) || ctx->omsg );
}

void wslay_event_shutdown_read ( wslay_event_context * ctx )
{
    ctx->read_enabled = 0;
}

void wslay_event_shutdown_write ( wslay_event_context * ctx )
{
    ctx->write_enabled = 0;
}

int wslay_event_get_read_enabled ( wslay_event_context * ctx )
{
    return ctx->read_enabled;
}

int wslay_event_get_write_enabled ( wslay_event_context * ctx )
{
    return ctx->write_enabled;
}

int wslay_event_get_close_received ( wslay_event_context * ctx )
{
    return ( ctx->close_status & WSLAY_CLOSE_RECEIVED ) > 0;
}

int wslay_event_get_close_sent ( wslay_event_context * ctx )
{
    return ( ctx->close_status & WSLAY_CLOSE_SENT ) > 0;
}

void wslay_event_config_set_no_buffering ( wslay_event_context * ctx, int val )
{
    if ( val ) {
        ctx->config |= WSLAY_CONFIG_NO_BUFFERING;
    } else {
        ctx->config &= ~WSLAY_CONFIG_NO_BUFFERING;
    }
}

void wslay_event_config_set_max_recv_msg_length ( wslay_event_context * ctx,
        uint64_t val )
{
    ctx->max_recv_msg_length = val;
}

uint16_t wslay_event_get_status_code_received ( wslay_event_context * ctx )
{
    return ctx->status_code_recv;
}

uint16_t wslay_event_get_status_code_sent ( wslay_event_context * ctx )
{
    return ctx->status_code_sent;
}

size_t wslay_event_get_queued_msg_count ( wslay_event_context * ctx )
{
    return ctx->queued_msg_count;
}

size_t wslay_event_get_queued_msg_length ( wslay_event_context * ctx )
{
    return ctx->queued_msg_length;
}

