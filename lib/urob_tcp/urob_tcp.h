/*
Copyright (c) 2023 Quantumboar <quantum@quantumboar.net>

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef __UROB_TCP_H__
#define __UROB_TCP_H__

#include "lwip/err.h"
#include "lwip/ip_addr.h"
#include <stdatomic.h>

#define MAX_UROB_TCP_MESSAGES (10)

typedef enum
{
    UROB_TCP_MESSAGE_STATE_NONE,
    UROB_TCP_MESSAGE_STATE_INIT,
    UROB_TCP_MESSAGE_STATE_SENDING,
    UROB_TCP_MESSAGE_STATE_SENT,
    UROB_TCP_MESSAGE_STATE_RECEIVING,
    UROB_TCP_MESSAGE_STATE_RECEIVED,
    UROB_TCP_MESSAGE_STATE_ERROR
} urob_tcp_message_state;

typedef enum
{
    UROB_TCP_MESSAGE_TYPE_NONE, // The message is not in use
    UROB_TCP_MESSAGE_TYPE_INCOMING,
    UROB_TCP_MESSAGE_TYPE_OUTGOING
} urob_tcp_message_type;

typedef struct _urob_tcp_message
{
    urob_tcp_message_type type;
    urob_tcp_message_state state;
    err_t err;

    bool free_payload_in_uninit;
    union
    {
        char * payload;
        struct pbuf * head_pbuf;
    };

    // These two aren't used when receiving
    int length; // bytes used by the message in memory
    size_t progress; // Amount written or read
} urob_tcp_message;

// Initializes a tcp message
// @param type: type of message (e.g. outgoing or incoming)
void urob_tcp_message_init(urob_tcp_message * tcp_message, urob_tcp_message_type type);

// Initializes the payload of a message as a string formatted following the printf notation.
// @discussion The psyload will be allocated from the heap, and released in the uninit function
void urob_tcp_message_payload_printf(urob_tcp_message * tcp_message, const char * format, ...);

void urob_tcp_message_uninit(urob_tcp_message * tcp_message);

typedef enum
{
  UROB_TCP_STATE_NONE = 0,
  UROB_TCP_STATE_INIT, // also handles ongoing connection
  UROB_TCP_STATE_CONNECTING,
  UROB_TCP_STATE_CONNECTED,
  UROB_TCP_STATE_ACCEPTING,
  UROB_TCP_STATE_ERROR
} urob_tcp_state;

typedef enum
{
    UROB_TCP_TYPE_NONE = 0,
    UROB_TCP_TYPE_CLIENT,
    UROB_TCP_TYPE_SERVER
} urob_tcp_type;

typedef struct
{
  struct netconn * conn;
  urob_tcp_type type;
  err_t err;
  urob_tcp_state state;

  urob_tcp_message * messages[MAX_UROB_TCP_MESSAGES];

  ip_addr_t address;
  int port;
} urob_tcp;

void urob_tcp_init_client(urob_tcp * tcp, ip_addr_t * address, int port);
void urob_tcp_uninit(urob_tcp * tcp);

// Add a message to the urob_tcp, if possible
// @discussion in case of failure the err field of message is set accordingly
void urob_tcp_add_message(urob_tcp * tcp, urob_tcp_message * message);

void urob_tcp_loop(urob_tcp * tcp);

#endif // __UROB_TCP_H__