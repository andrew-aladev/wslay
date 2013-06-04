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
#ifndef WSLAY_FRAME_H
#define WSLAY_FRAME_H

#include "wslay.h"

enum wslay_frame_state {
    PREP_HEADER,
    SEND_HEADER,
    SEND_PAYLOAD,
    RECV_HEADER1,
    RECV_PAYLOADLEN,
    RECV_EXT_PAYLOADLEN,
    RECV_MASKKEY,
    RECV_PAYLOAD
};

struct wslay_frame_opcode_memo {
    uint8_t fin;
    uint8_t opcode;
    uint8_t rsv;
};

typedef struct wslay_frame_context_t {
    uint8_t ibuf[4096];
    uint8_t * ibufmark;
    uint8_t * ibuflimit;
    struct wslay_frame_opcode_memo iom;
    uint64_t ipayloadlen;
    uint64_t ipayloadoff;
    uint8_t imask;
    uint8_t imaskkey[4];
    enum wslay_frame_state istate;
    size_t ireqread;

    uint8_t oheader[14];
    uint8_t * oheadermark;
    uint8_t * oheaderlimit;
    uint64_t opayloadlen;
    uint64_t opayloadoff;
    uint8_t omask;
    uint8_t omaskkey[4];
    enum wslay_frame_state ostate;

    struct wslay_frame_callbacks callbacks;
    void *user_data;
} wslay_frame_context;

/*
 * Initializes ctx using given callbacks and user_data.
 * This function allocates memory for struct wslay_frame_context and stores the result to *ctx.
 * The callback functions specified in callbacks are copied to ctx.
 * user_data is stored in ctx and it will be passed to callback functions.
 */
uint8_t wslay_frame_context_init ( void * ctx, wslay_frame_context ** result_frame_ctx, const struct wslay_frame_callbacks * callbacks, void * user_data );

/*
 * Send WebSocket frame specified in iocb.
 * ctx must be initialized using wslay_frame_context_init() function.
 * iocb->fin must be 1 if this is a fin frame, otherwise 0.
 * iocb->rsv is reserved bits.
 * iocb->opcode must be the opcode of this frame.
 * iocb->mask must be 1 if this is masked frame, otherwise 0.
 * iocb->payload_length is the payload_length of this frame.
 * iocb->data must point to the payload data to be sent.
 * iocb->data_length must be the length of the data.
 * This function calls recv_callback function if it needs to send bytes.
 * This function calls gen_mask_callback function if it needs new mask key.
 * This function returns the number of payload bytes sent.
 * Please note that it does not include any number of header bytes.
 * If it cannot send any single bytes of payload, it returns WSLAY_ERR_WANT_WRITE.
 * If the library detects error in iocb, this function returns WSLAY_ERR_INVALID_ARGUMENT.
 * If callback functions report a failure, this function returns WSLAY_ERR_INVALID_CALLBACK.
 * This function does not always send all given data in iocb.
 * If there are remaining data to be sent, adjust data and data_length in iocb accordingly and call this function again.
 */
int16_t wslay_frame_send ( wslay_frame_context * ctx, struct wslay_frame_iocb * iocb, size_t * length );

/*
 * Receives WebSocket frame and stores it in iocb.
 * This function returns the number of payload bytes received.
 * This does not include header bytes.
 * In this case, iocb will be populated as follows:
 * iocb->fin is 1 if received frame is fin frame, otherwise 0.
 * iocb->rsv is reserved bits of received frame.
 * iocb->opcode is opcode of received frame.
 * iocb->mask is 1 if received frame is masked, otherwise 0.
 * iocb->payload_length is the payload length of received frame.
 * iocb->data is pointed to the buffer containing received payload data.
 * This buffer is allocated by the library and must be read-only.
 * iocb->data_length is the number of payload bytes recieved.
 * This function calls recv_callback if it needs to receive additional bytes.
 * If it cannot receive any single bytes of payload, it returns WSLAY_ERR_WANT_READ.
 * If the library detects protocol violation in a received frame, this function returns WSLAY_ERR_PROTO.
 * If callback functions report a failure, this function returns WSLAY_ERR_INVALID_CALLBACK.
 * This function does not always receive whole frame in a single call.
 * If there are remaining data to be received, call this function again.
 * This function ensures frame alignment.
 */
ssize_t wslay_frame_recv ( wslay_frame_context * ctx, struct wslay_frame_iocb * iocb );

#endif
