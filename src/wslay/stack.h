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
#ifndef WSLAY_STACK_H
#define WSLAY_STACK_H

#include "stdbool.h"

#include <talloc2/tree.h>
#include <talloc2/ext/destructor.h>

#include "wslay.h"

typedef struct wslay_stack_cell_t {
    void * data;
    struct wslay_stack_cell_t * next;
} wslay_stack_cell;

typedef struct wslay_stack_t {
    wslay_stack_cell * top;
} wslay_stack;

inline
uint8_t wslay_stack_free ( void * data )
{
    wslay_stack * stack = data;
    if ( stack == NULL ) {
        return 1;
    }
    wslay_stack_cell * cell = stack->top;
    wslay_stack_cell * next_cell;
    while ( cell ) {
        next_cell = cell->next;
        free ( cell );
        cell = next_cell;
    }
    return 0;
}

inline
wslay_stack * wslay_stack_new ( void * ctx )
{
    wslay_stack * stack = talloc ( ctx, sizeof ( wslay_stack ) );
    if ( stack == NULL ) {
        return NULL;
    }
    if ( talloc_set_destructor ( stack, wslay_stack_free ) != 0 ) {
        return NULL;
    }
    stack->top = NULL;
    return stack;
}

inline
uint8_t wslay_stack_push ( wslay_stack * stack, void * data )
{
    wslay_stack_cell * new_cell = malloc ( sizeof ( wslay_stack_cell ) );
    if ( new_cell == NULL ) {
        return 1;
    }
    new_cell->data = data;
    new_cell->next = stack->top;
    stack->top = new_cell;
    return 0;
}

inline
uint8_t wslay_stack_pop ( wslay_stack * stack )
{
    wslay_stack_cell * top = stack->top;
    if ( top == NULL ) {
        return 1;
    }
    stack->top = top->next;
    free ( top );
    return 0;
}

inline
void * wslay_stack_top ( wslay_stack * stack )
{
    return stack->top->data;
}

inline
bool wslay_stack_is_empty ( wslay_stack *stack )
{
    return stack->top == NULL;
}


#endif