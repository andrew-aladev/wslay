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
#ifndef WSLAY_EVENT_H
#define WSLAY_EVENT_H

#include "wslay.h"
#include "frame.h"
#include "queue.h"

struct wslay_event_byte_chunk {
    uint8_t * data;
    size_t data_length;
};

struct wslay_event_imsg {
    uint8_t fin;
    uint8_t rsv;
    uint8_t opcode;
    uint32_t utf8state;
    wslay_queue * chunks;
    size_t msg_length;
};

enum wslay_event_msg_type {
    WSLAY_NON_FRAGMENTED,
    WSLAY_FRAGMENTED
};

enum wslay_event_close_status {
    WSLAY_CLOSE_RECEIVED = 1,
    WSLAY_CLOSE_QUEUED   = 1 << 1,
    WSLAY_CLOSE_SENT     = 1 << 2
};

enum wslay_event_config {
    WSLAY_CONFIG_NO_BUFFERING = 1
};

struct wslay_event_context;
// Pointer to the event-based API context
typedef struct wslay_event_context * wslay_event_context_ptr;

struct wslay_event_frame_user_data {
    wslay_event_context_ptr ctx;
    void * user_data;
};

struct wslay_event_on_msg_recv_arg {
    // reserved bits: rsv = (RSV1 << 2) | (RSV2 << 1) | RSV3
    uint8_t rsv;
    // opcode
    uint8_t opcode;
    // received message
    const uint8_t *msg;
    // message length
    size_t msg_length;
    // status code iff opcode == WSLAY_CONNECTION_CLOSE.
    // If no status code is included in the close control frame, it is set to 0.
    uint16_t status_code;
};

// Callback function invoked by wslay_event_recv() when a message is completely received.
typedef void ( * wslay_event_on_msg_recv_callback ) ( wslay_event_context_ptr ctx, const struct wslay_event_on_msg_recv_arg * arg, void * user_data );

struct wslay_event_on_frame_recv_start_arg {
    // fin bit; 1 for final frame, or 0.
    uint8_t fin;
    // reserved bits: rsv = (RSV1 << 2) | (RSV2 << 1) | RSV3
    uint8_t rsv;
    // opcode of the frame
    uint8_t opcode;
    // payload length of ths frame
    uint64_t payload_length;
};

// Callback function invoked by wslay_event_recv() when a new frame starts to be received.
// This callback function is only invoked once for each frame.
typedef void ( *wslay_event_on_frame_recv_start_callback ) ( wslay_event_context_ptr ctx, const struct wslay_event_on_frame_recv_start_arg * arg, void * user_data );

struct wslay_event_on_frame_recv_chunk_arg {
    // chunk of payload data
    const uint8_t * data;
    // length of data
    size_t data_length;
};

// Callback function invoked by wslay_event_recv() when a chunk of frame payload is received.
typedef void ( *wslay_event_on_frame_recv_chunk_callback ) ( wslay_event_context_ptr ctx, const struct wslay_event_on_frame_recv_chunk_arg * arg, void * user_data );

// Callback function invoked by wslay_event_recv() when a frame is completely received.
typedef void ( *wslay_event_on_frame_recv_end_callback ) ( wslay_event_context_ptr ctx, void * user_data );

/*
 * Callback function invoked by wslay_event_recv() when it wants to receive more data from peer.
 * The implementation of this callback function must read data at most len bytes from peer
 * and store them in buf and return the number of bytes read.
 * flags is always 0 in this version.
 *
 * If there is an error, return -1 and set error code WSLAY_ERR_CALLBACK_FAILURE using wslay_event_set_error().
 * Wslay event-based API on the whole assumes non-blocking I/O.
 * If the cause of error is EAGAIN or EWOULDBLOCK, set WSLAY_ERR_WOULDBLOCK instead.
 * This is important because it tells wslay_event_recv() to stop receiving further data and return.
 */
typedef ssize_t ( * wslay_event_recv_callback ) ( wslay_event_context_ptr ctx, uint8_t * buf, size_t len, int flags, void * user_data );

/*
 * Callback function invoked by wslay_event_send() when it wants to send more data to peer.
 * The implementation of this callback function must send data at most len bytes to peer and return the number of bytes sent.
 * flags is the bitwise OR of zero or more of the following flag:
 *
 * WSLAY_MSG_MORE
 *   There is more data to send
 *
 * It provides some hints to tune performance and behaviour.
 *
 * If there is an error, return -1 and set error code WSLAY_ERR_CALLBACK_FAILURE using wslay_event_set_error().
 * Wslay event-based API on the whole assumes non-blocking I/O.
 * If the cause of error is EAGAIN or EWOULDBLOCK, set WSLAY_ERR_WOULDBLOCK instead.
 * This is important because it tells wslay_event_send() to stop sending data and return.
 */
typedef ssize_t ( * wslay_event_send_callback ) ( wslay_event_context_ptr ctx, const uint8_t * data, size_t len, int flags, void * user_data );

// Callback function invoked by wslay_event_send() when it wants new mask key.
// As described in RFC6455, only the traffic from WebSocket client is masked,
// so this callback function is only needed if an event-based API is initialized for WebSocket client use.
typedef int ( *wslay_event_genmask_callback ) ( wslay_event_context_ptr ctx, uint8_t * buf, size_t len, void * user_data );

struct wslay_event_callbacks {
    wslay_event_recv_callback recv_callback;
    wslay_event_send_callback send_callback;
    wslay_event_genmask_callback genmask_callback;
    wslay_event_on_frame_recv_start_callback on_frame_recv_start_callback;
    wslay_event_on_frame_recv_chunk_callback on_frame_recv_chunk_callback;
    wslay_event_on_frame_recv_end_callback on_frame_recv_end_callback;
    wslay_event_on_msg_recv_callback on_msg_recv_callback;
};

struct wslay_event_context {
    // config status, bitwise OR of enum wslay_event_config values
    uint32_t config;
    // maximum message length that can be received
    uint64_t max_recv_msg_length;
    // 1 if initialized for server, otherwise 0
    uint8_t server;
    // bitwise OR of enum wslay_event_close_status values
    uint8_t close_status;
    // status code in received close control frame
    uint16_t status_code_recv;
    // status code in sent close control frame
    uint16_t status_code_sent;
    wslay_frame_context_ptr frame_ctx;
    // 1 if reading is enabled, otherwise 0.
    // Upon receiving close control frame this value set to 0.
    // If any errors in read operation will also set this value to 0. */
    uint8_t read_enabled;
    // 1 if writing is enabled, otherwise 0 Upon completing sending close control frame, this value set to 0.
    // If any errors in write opration will also set this value to 0.
    uint8_t write_enabled;
    // imsg buffer to allow interleaved control frame between non-control frames.
    struct wslay_event_imsg imsgs[2];
    // Pointer to imsgs to indicate current used buffer.
    struct wslay_event_imsg * imsg;
    // payload length of frame currently being received.
    uint64_t ipayloadlen;
    // next byte offset of payload currently being received.
    uint64_t ipayloadoff;
    // error value set by user callback
    int error;
    // Pointer to the message currently being sent. NULL if no message is currently sent.
    struct wslay_event_omsg * omsg;
    // Queue for non-control frames
    wslay_queue * send_queue;
    // Queue for control frames
    wslay_queue * send_ctrl_queue;
    // Size of send_queue + size of send_ctrl_queue
    size_t queued_msg_count;
    // The sum of message length in send_queue
    size_t queued_msg_length;
    // Buffer used for fragmented messages
    uint8_t obuf[4096];
    uint8_t * obuflimit;
    uint8_t * obufmark;
    // payload length of frame currently being sent.
    uint64_t opayloadlen;
    // next byte offset of payload currently being sent.
    uint64_t opayloadoff;
    struct wslay_event_callbacks callbacks;
    struct wslay_event_frame_user_data frame_user_data;
    void * user_data;
};

/*
 * Initializes ctx as WebSocket Server.
 * user_data is an arbitrary pointer, which is directly passed to each callback functions as user_data argument.
 *
 * On success, returns 0. On error, returns one of following negative values:
 *
 * WSLAY_ERR_NOMEM
 *   Out of memory.
 */
int wslay_event_context_server_init ( wslay_event_context_ptr * ctx, const struct wslay_event_callbacks * callbacks, void * user_data );

/*
 * Initializes ctx as WebSocket client. user_data is an arbitrary pointer, which is directly passed to each callback functions as user_data argument.
 *
 * On success, returns 0. On error, returns one of following negative values:
 *
 * WSLAY_ERR_NOMEM
 *   Out of memory.
 */
int wslay_event_context_client_init ( wslay_event_context_ptr * ctx, const struct wslay_event_callbacks * callbacks, void * user_data );

// Releases allocated resources for ctx.
void wslay_event_context_free ( wslay_event_context_ptr ctx );

/*
 * Enables or disables buffering of an entire message for non-control frames.
 * If val is 0, buffering is enabled. Otherwise, buffering is disabled.
 * If wslay_event_on_msg_recv_callback is invoked when buffering is disabled, the msg_length member of struct wslay_event_on_msg_recv_arg is set to 0.
 *
 * The control frames are always buffered regardless of this function call.
 *
 * This function must not be used after the first invocation of wslay_event_recv() function.
 */
void wslay_event_config_set_no_buffering ( wslay_event_context_ptr ctx, int val );

/*
 * Sets maximum length of a message that can be received.
 * The length of message is checked by wslay_event_recv() function.
 * If the length of a message is larger than this value, reading operation is disabled (same effect with wslay_event_shutdown_read() call) and close control frame with WSLAY_CODE_MESSAGE_TOO_BIG is queued.
 * If buffering for non-control frames is disabled, the library checks each frame payload length and does not check length of entire message.
 *
 * The default value is (1 << 31) - 1.
 */
void wslay_event_config_set_max_recv_msg_length ( wslay_event_context_ptr ctx, uint64_t val );

// Sets callbacks to ctx.
// The callbacks previouly set by this function or wslay_event_context_server_init() or wslay_event_context_client_init() are replaced with callbacks.
void wslay_event_config_set_callbacks ( wslay_event_context_ptr ctx, const struct wslay_event_callbacks * callbacks );

/*
 * Receives messages from peer. When receiving messages, it uses wslay_event_recv_callback function.
 * Single call of this function receives multiple messages until wslay_event_recv_callback function sets error code WSLAY_ERR_WOULDBLOCK.
 *
 * When close control frame is received, this function automatically queues close control frame.
 * Also this function calls wslay_event_set_read_enabled() with second argument 0 to disable further read from peer.
 *
 * When ping control frame is received, this function automatically queues pong control frame.
 *
 * In case of a fatal errror which leads to negative return code,
 * this function calls wslay_event_set_read_enabled() with second argument 0 to disable further read from peer.
 *
 * wslay_event_recv() returns 0 if it succeeds, or one of the following negative error codes:
 *
 * WSLAY_ERR_CALLBACK_FAILURE
 *   User defined callback function is failed.
 *
 * WSLAY_ERR_NOMEM
 *   Out of memory.
 *
 * When negative error code is returned, application must not make any further call of wslay_event_recv() and must close WebSocket connection.
 */
int wslay_event_recv ( wslay_event_context_ptr ctx );

/*
 * Sends queued messages to peer.
 * When sending a message, it uses wslay_event_send_callback function.
 * Single call of wslay_event_send() sends multiple messages until wslay_event_send_callback sets error code WSLAY_ERR_WOULDBLOCK.
 *
 * If ctx is initialized for WebSocket client use, wslay_event_send() uses wslay_event_genmask_callback to get new mask key.
 *
 * When a message queued using wslay_event_queue_fragmented_msg() is sent,
 * wslay_event_send() invokes wslay_event_fragmented_msg_callback for that message.
 *
 * After close control frame is sent,
 * this function calls wslay_event_set_write_enabled() with second argument 0 to disable further transmission to peer.
 *
 * If there are any pending messages, wslay_event_want_write() returns 1, otherwise returns 0.
 *
 * In case of a fatal errror which leads to negative return code,
 * this function calls wslay_event_set_write_enabled() with second argument 0 to disable further transmission to peer.
 *
 * wslay_event_send() returns 0 if it succeeds, or one of the following negative error codes:
 *
 * WSLAY_ERR_CALLBACK_FAILURE
 *   User defined callback function is failed.
 *
 * WSLAY_ERR_NOMEM
 *   Out of memory.
 *
 * When negative error code is returned, application must not make any further call of wslay_event_send() and must close WebSocket connection.
 */
int wslay_event_send ( wslay_event_context_ptr ctx );

struct wslay_event_msg {
    uint8_t opcode;
    const uint8_t *msg;
    size_t msg_length;
};

/*
 * Queues message specified in arg.
 *
 * This function supports both control and non-control messages and the given message is sent without fragmentation.
 * If fragmentation is needed, use wslay_event_queue_fragmented_msg() function instead.
 *
 * This function just queues a message and does not send it.
 * wslay_event_send() function call sends these queued messages.
 *
 * wslay_event_queue_msg() returns 0 if it succeeds, or returns the following negative error codes:
 *
 * WSLAY_ERR_NO_MORE_MSG
 *   Could not queue given message. The one of possible reason is that
 *   close control frame has been queued/sent and no further queueing
 *   message is not allowed.
 *
 * WSLAY_ERR_INVALID_ARGUMENT
 *   The given message is invalid.
 *
 * WSLAY_ERR_NOMEM
 *   Out of memory.
 */
int wslay_event_queue_msg ( wslay_event_context_ptr ctx, const struct wslay_event_msg * arg );

// Specify "source" to generate message.
union wslay_event_msg_source {
    int fd;
    void *data;
};

/*
 * Callback function called by wslay_event_send() to read message data from source.
 * The implementation of wslay_event_fragmented_msg_callback must store at most len bytes of data to buf and return the number of stored bytes.
 * If all data is read (i.e., EOF), set *eof to 1.
 * If no data can be generated at the moment, return 0.
 * If there is an error, return -1 and set error code WSLAY_ERR_CALLBACK_FAILURE using wslay_event_set_error().
 */
typedef ssize_t ( *wslay_event_fragmented_msg_callback ) ( wslay_event_context_ptr ctx, uint8_t * buf, size_t len, const union wslay_event_msg_source * source, int * eof, void * user_data );

struct wslay_event_omsg {
    uint8_t fin;
    uint8_t opcode;
    enum wslay_event_msg_type type;

    uint8_t * data;
    size_t data_length;

    union wslay_event_msg_source source;
    wslay_event_fragmented_msg_callback read_callback;
};

struct wslay_event_fragmented_msg {
    // opcode
    uint8_t opcode;
    // "source" to generate message data
    union wslay_event_msg_source source;
    // Callback function to read message data from source.
    wslay_event_fragmented_msg_callback read_callback;
};

/*
 * Queues a fragmented message specified in arg.
 *
 * This function supports non-control messages only.
 * For control frames, use wslay_event_queue_msg() or wslay_event_queue_close().
 *
 * This function just queues a message and does not send it.
 * wslay_event_send() function call sends these queued messages.
 *
 * wslay_event_queue_fragmented_msg() returns 0 if it succeeds, or returns the following negative error codes:
 *
 * WSLAY_ERR_NO_MORE_MSG
 *   Could not queue given message.
 *   The one of possible reason is that close control frame has been queued/sent
 *   and no further queueing message is not allowed.
 *
 * WSLAY_ERR_INVALID_ARGUMENT
 *   The given message is invalid.
 *
 * WSLAY_ERR_NOMEM
 *   Out of memory.
 */
int wslay_event_queue_fragmented_msg ( wslay_event_context_ptr ctx, const struct wslay_event_fragmented_msg * arg );

/*
 * Queues close control frame.
 * This function is provided just for convenience.
 * wslay_event_queue_msg() can queue a close control frame as well.
 * status_code is the status code of close control frame.
 * reason is the close reason encoded in UTF-8.
 * reason_length is the length of reason in bytes.
 * reason_length must be less than 123 bytes.
 *
 * If status_code is 0, reason and reason_length is not used and close control frame with zero-length payload will be queued.
 *
 * This function just queues a message and does not send it.
 * wslay_event_send() function call sends these queued messages.
 *
 * wslay_event_queue_close() returns 0 if it succeeds, or returns the following negative error codes:
 *
 * WSLAY_ERR_NO_MORE_MSG
 *   Could not queue given message. The one of possible reason is that
 *   close control frame has been queued/sent and no further queueing
 *   message is not allowed.
 *
 * WSLAY_ERR_INVALID_ARGUMENT
 *   The given message is invalid.
 *
 * WSLAY_ERR_NOMEM
 *   Out of memory.
 */
int wslay_event_queue_close ( wslay_event_context_ptr ctx, uint16_t status_code, const uint8_t * reason, size_t reason_length );

// Sets error code to tell the library there is an error.
// This function is typically used in user defined callback functions.
// See the description of callback function to know which error code should be used.
void wslay_event_set_error ( wslay_event_context_ptr ctx, int val );

// Query whehter the library want to read more data from peer.
// wslay_event_want_read() returns 1 if the library want to read more data from peer, or returns 0.
int wslay_event_want_read ( wslay_event_context_ptr ctx );

// Query whehter the library want to send more data to peer.
// wslay_event_want_write() returns 1 if the library want to send more data to peer, or returns 0.
int wslay_event_want_write ( wslay_event_context_ptr ctx );

// Prevents the event-based API context from reading any further data from peer.
// This function may be used with wslay_event_queue_close()
// if the application detects error in the data received and wants to fail WebSocket connection.
void wslay_event_shutdown_read ( wslay_event_context_ptr ctx );

// Prevents the event-based API context from sending any further data to peer.
void wslay_event_shutdown_write ( wslay_event_context_ptr ctx );

// Returns 1 if the event-based API context allows read operation, or return 0.
// After wslay_event_shutdown_read() is called, wslay_event_get_read_enabled() returns 0.
int wslay_event_get_read_enabled ( wslay_event_context_ptr ctx );

// Returns 1 if the event-based API context allows write operation, or return 0.
// After wslay_event_shutdown_write() is called, wslay_event_get_write_enabled() returns 0.
int wslay_event_get_write_enabled ( wslay_event_context_ptr ctx );

// Returns 1 if a close control frame has been received from peer, or returns 0.
int wslay_event_get_close_received ( wslay_event_context_ptr ctx );

// Returns 1 if a close control frame has been sent to peer, or returns 0.
int wslay_event_get_close_sent ( wslay_event_context_ptr ctx );

// Returns status code received in close control frame.
// If no close control frame has not been received, returns WSLAY_CODE_ABNORMAL_CLOSURE.
// If received close control frame has no status code, returns WSLAY_CODE_NO_STATUS_RCVD.
uint16_t wslay_event_get_status_code_received ( wslay_event_context_ptr ctx );

// Returns status code sent in close control frame.
// If no close control frame has not been sent, returns WSLAY_CODE_ABNORMAL_CLOSURE.
// If sent close control frame has no status code, returns WSLAY_CODE_NO_STATUS_RCVD.
uint16_t wslay_event_get_status_code_sent ( wslay_event_context_ptr ctx );

// Returns the number of queued messages.
size_t wslay_event_get_queued_msg_count ( wslay_event_context_ptr ctx );

// Returns the sum of queued message length.
// It only counts the message length queued using wslay_event_queue_msg() or wslay_event_queue_close().
size_t wslay_event_get_queued_msg_length ( wslay_event_context_ptr ctx );

#endif