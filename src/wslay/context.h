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
#ifndef WSLAY_CONTEXT_H
#define WSLAY_CONTEXT_H

#include "event.h"

wslay_event_context * wslay_event_context_new ( void * ctx, const struct wslay_event_callbacks * callbacks, void * user_data );

inline
wslay_event_context * wslay_server_new ( void * ctx, const struct wslay_event_callbacks * callbacks, void * user_data ) {
    wslay_event_context * context = wslay_event_context_new ( ctx, callbacks, user_data );
    if ( context == NULL ) {
        return NULL;
    }
    context->server = true;
    return context;
}

inline
wslay_event_context * wslay_client_new ( void * ctx, const struct wslay_event_callbacks * callbacks, void * user_data ) {
    wslay_event_context * context = wslay_event_context_new ( ctx, callbacks, user_data );
    if ( context == NULL ) {
        return NULL;
    }
    context->server = false;
    return context;
}

inline
ssize_t wslay_event_frame_recv_callback ( uint8_t * buf, size_t len, int flags, void * _user_data )
{
    struct wslay_event_frame_user_data * user_data = _user_data;
    return user_data->ctx->callbacks.recv_callback ( user_data->ctx, buf, len, flags, user_data->user_data );
}

inline
ssize_t wslay_event_frame_send_callback ( const uint8_t * data, size_t len, int flags, void * _user_data, bool user_data_sending )
{
    struct wslay_event_frame_user_data * user_data = _user_data;
    return user_data->ctx->callbacks.send_callback ( user_data->ctx, data, len, flags, user_data->user_data, user_data_sending );
}

inline
int wslay_event_frame_genmask_callback ( uint8_t * buf, size_t len, void * _user_data )
{
    struct wslay_event_frame_user_data * user_data = _user_data;
    return user_data->ctx->callbacks.genmask_callback ( user_data->ctx, buf, len, user_data->user_data );
}

#endif