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

#include <limits.h>

#include "context.h"
#include "frame.h"

#include <talloc2/tree.h>

extern inline
ssize_t wslay_event_frame_recv_callback ( uint8_t * buf, size_t len, int flags, void * _user_data );

extern inline
ssize_t wslay_event_frame_send_callback ( const uint8_t * data, size_t len, int flags, void * _user_data );

extern inline
int wslay_event_frame_genmask_callback ( uint8_t * buf, size_t len, void * _user_data );

wslay_event_context * wslay_event_context_new ( void * ctx, const struct wslay_event_callbacks * callbacks, void * user_data )
{
    wslay_event_context * context = talloc_zero ( ctx, sizeof ( wslay_event_context ) );
    if ( context == NULL ) {
        return NULL;
    }
    struct wslay_frame_callbacks frame_callbacks = { wslay_event_frame_send_callback, wslay_event_frame_recv_callback, wslay_event_frame_genmask_callback };
    context->callbacks = * callbacks;
    context->user_data = user_data;

    context->frame_user_data.ctx       = context;
    context->frame_user_data.user_data = user_data;

    wslay_frame_context * frame_ctx = wslay_frame_context_new ( context, &frame_callbacks, &context->frame_user_data );
    if ( frame_ctx == NULL ) {
        talloc_free ( context );
        return NULL;
    }
    context->frame_ctx = frame_ctx;
    
    context->read_enabled = context->write_enabled = 1;
    context->send_queue   = wslay_queue_new ( context );
    if ( context->send_queue == NULL ) {
        talloc_free ( context );
        return NULL;
    }
    context->send_ctrl_queue = wslay_queue_new ( context );
    if ( context->send_ctrl_queue == NULL ) {
        talloc_free ( context );
        return NULL;
    }
    context->queued_msg_count  = 0;
    context->queued_msg_length = 0;

    uint8_t i;
    for ( i = 0; i < 2; ++i ) {
        wslay_event_imsg_reset ( & context->imsgs[i] );
        context->imsgs[i].chunks = wslay_queue_new ( context );
        if ( context->imsgs[i].chunks == NULL ) {
            talloc_free ( context );
            return NULL;
        }
    }

    context->imsg     = & context->imsgs[0];
    context->obufmark = context->obuflimit = context->obuf;
    context->status_code_sent = WSLAY_CODE_ABNORMAL_CLOSURE;
    context->status_code_recv = WSLAY_CODE_ABNORMAL_CLOSURE;
    context->max_recv_msg_length = UINT64_MAX;

    return context;
}

extern inline
wslay_event_context * wslay_server_new ( void * ctx, const struct wslay_event_callbacks * callbacks, void * user_data );

extern inline
wslay_event_context * wslay_client_new ( void * ctx, const struct wslay_event_callbacks * callbacks, void * user_data );