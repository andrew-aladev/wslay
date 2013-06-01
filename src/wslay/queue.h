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
#ifndef WSLAY_QUEUE_H
#define WSLAY_QUEUE_H

#include <stdbool.h>

#include <talloc2/tree.h>
#include <talloc2/ext/destructor.h>

#include "wslay.h"

typedef struct wslay_queue_cell_t {
    void * data;
    struct wslay_queue_cell_t * next;
} wslay_queue_cell;

typedef struct wslay_queue_t {
    wslay_queue_cell * top;
    wslay_queue_cell * tail;
} wslay_queue;

inline
uint8_t wslay_queue_free ( void * data )
{
    wslay_queue * queue = data;
    if ( queue == NULL ) {
        return 1;
    }

    wslay_queue_cell * cell = queue->top;
    wslay_queue_cell * next_cell;
    while ( cell != NULL ) {
        next_cell = cell->next;
        free ( cell );
        cell = next_cell;
    }
    return 0;
}

inline
wslay_queue * wslay_queue_new ( void * ctx )
{
    wslay_queue * queue = talloc ( ctx, sizeof ( wslay_queue ) );
    if ( queue == NULL ) {
        return NULL;
    }
    if ( talloc_set_destructor ( queue, wslay_queue_free ) != 0 ) {
        return NULL;
    }
    queue->top = queue->tail = NULL;
    return queue;
}

inline
uint8_t wslay_queue_push ( wslay_queue * queue, void * data )
{
    wslay_queue_cell * new_cell = malloc ( sizeof ( wslay_queue_cell ) );
    if ( new_cell == NULL ) {
        return 1;
    }
    new_cell->data = data;
    new_cell->next = NULL;
    if ( queue->tail ) {
        queue->tail->next = new_cell;
        queue->tail = new_cell;

    } else {
        queue->top = queue->tail = new_cell;
    }
    return 0;
}

inline
uint8_t wslay_queue_push_front ( wslay_queue * queue, void * data )
{
    wslay_queue_cell * new_cell = malloc ( sizeof ( wslay_queue_cell ) );
    if ( new_cell == NULL ) {
        return 1;
    }
    new_cell->data = data;
    new_cell->next = queue->top;
    queue->top = new_cell;
    if ( queue->tail == NULL ) {
        queue->tail = queue->top;
    }
    return 0;
}

inline
uint8_t wslay_queue_pop ( wslay_queue * queue )
{
    wslay_queue_cell * top = queue->top;
    if ( top == NULL ) {
        return 1;
    }
    queue->top = top->next;
    if ( top == queue->tail ) {
        queue->tail = NULL;
    }
    free ( top );
    return 0;
}

inline
void * wslay_queue_top ( wslay_queue * queue )
{
    return queue->top->data;
}

inline
void * wslay_queue_tail ( wslay_queue * queue )
{
    return queue->tail->data;
}

inline
bool wslay_queue_is_empty ( wslay_queue * queue )
{
    return queue->top == NULL;
}

#endif
