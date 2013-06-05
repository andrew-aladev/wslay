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
#ifndef WSLAY_H
#define WSLAY_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>

enum wslay_error {
    WSLAY_ERR_WANT_READ        = -100,
    WSLAY_ERR_WANT_WRITE       = -101,
    WSLAY_ERR_PROTO            = -200,
    WSLAY_ERR_INVALID_ARGUMENT = -300,
    WSLAY_ERR_INVALID_CALLBACK = -301,
    WSLAY_ERR_NO_MORE_MSG      = -302,
    WSLAY_ERR_CALLBACK_FAILURE = -400,
    WSLAY_ERR_WOULDBLOCK       = -401,
    WSLAY_ERR_NOMEM            = -500
};

enum wslay_status_code {
    WSLAY_CODE_NORMAL_CLOSURE             = 1000,
    WSLAY_CODE_GOING_AWAY                 = 1001,
    WSLAY_CODE_PROTOCOL_ERROR             = 1002,
    WSLAY_CODE_UNSUPPORTED_DATA           = 1003,
    WSLAY_CODE_NO_STATUS_RCVD             = 1005,
    WSLAY_CODE_ABNORMAL_CLOSURE           = 1006,
    WSLAY_CODE_INVALID_FRAME_PAYLOAD_DATA = 1007,
    WSLAY_CODE_POLICY_VIOLATION           = 1008,
    WSLAY_CODE_MESSAGE_TOO_BIG            = 1009,
    WSLAY_CODE_MANDATORY_EXT              = 1010,
    WSLAY_CODE_INTERNAL_SERVER_ERROR      = 1011,
    WSLAY_CODE_TLS_HANDSHAKE              = 1015
};

enum wslay_io_flags {
    // There is more data to send.
    WSLAY_MSG_MORE = 1
};

/*
 * Callback function used by wslay_frame_send() function when it needs to send data.
 * The implementation of this function must send at most len bytes of data in data.
 * flags is the bitwise OR of zero or more of the following flag:
 *
 * WSLAY_MSG_MORE
 *   There is more data to send
 *
 * It provides some hints to tune performance and behaviour.
 * user_data is one given in wslay_frame_context_init() function.
 * The implementation of this function must return the number of bytes sent.
 * If there is an error, return -1. The return value 0 is also treated an error by the library.
 */
typedef ssize_t ( * wslay_frame_send_callback ) ( const uint8_t * data, size_t len, int flags, void * user_data );

/*
 * Callback function used by wslay_frame_recv() function when it needs more data.
 * The implementation of this function must fill at most len bytes of data into buf.
 * The memory area of buf is allocated by library and not be freed by the application code.
 * flags is always 0 in this version.
 * user_data is one given in wslay_frame_context_init() function.
 * The implementation of this function must return the number of bytes filled.
 * If there is an error, return -1.
 * The return value 0 is also treated an error by the library.
 */
typedef ssize_t ( * wslay_frame_recv_callback ) ( uint8_t * buf, size_t len, int flags, void * user_data );

/*
 * Callback function used by wslay_frame_send() function when it needs new mask key.
 * The implementation of this function must write exactly len bytes of mask key to buf.
 * user_data is one given in wslay_frame_context_init() function.
 * The implementation of this function return 0 on success.
 * If there is an error, return -1.
 */
typedef int ( * wslay_frame_genmask_callback ) ( uint8_t * buf, size_t len, void * user_data );

struct wslay_frame_callbacks {
    wslay_frame_send_callback send_callback;
    wslay_frame_recv_callback recv_callback;
    wslay_frame_genmask_callback genmask_callback;
};

enum wslay_opcode {
    WSLAY_CONTINUATION_FRAME = 0x0,
    WSLAY_TEXT_FRAME         = 0x1,
    WSLAY_BINARY_FRAME       = 0x2,
    WSLAY_CONNECTION_CLOSE   = 0x8,
    WSLAY_PING               = 0x9,
    WSLAY_PONG               = 0xa
};

// Macro that returns 1 if opcode is control frame opcode, otherwise returns 0.
#define wslay_is_ctrl_frame(opcode) ((opcode >> 3) & 1)

// Macros that returns reserved bits: RSV1, RSV2, RSV3.
// These macros assumes that rsv is constructed by ((RSV1 << 2) | (RSV2 << 1) | RSV3)
#define wslay_get_rsv1(rsv) ((rsv >> 2) & 1)
#define wslay_get_rsv2(rsv) ((rsv >> 1) & 1)
#define wslay_get_rsv3(rsv) (rsv & 1)

struct wslay_frame_iocb {
    // 1 for fragmented final frame, 0 for otherwise
    uint8_t fin;
    // reserved 3 bits.  rsv = ((RSV1 << 2) | (RSV << 1) | RSV3).
    // RFC6455 requires 0 unless extensions are negotiated.
    uint8_t rsv;
    // 4 bit opcode
    uint8_t opcode;
    // payload length [0, 2**63-1]
    uint64_t payload_length;
    bool mask;
    // part of payload data
    const uint8_t *data;
    // bytes of data defined above
    size_t data_length;
};

#endif