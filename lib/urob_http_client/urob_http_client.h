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

#ifndef __UROB_HTTP_CLIENT_H__
#define __UROB_HTTP_CLIENT_H__

#include "lwip/ip_addr.h"
#include "lwip/err.h"
#include <stdatomic.h>

typedef enum
{
  CLIENT_STATE_NONE = 0,
  CLIENT_STATE_INIT, // also handles ongoing connection
  CLIENT_STATE_CONNECTING,
  CLIENT_STATE_CONNECTED,
  CLIENT_STATE_SENDING_REQ,
  CLIENT_STATE_WAIT_RESP,
  CLIENT_STATE_RESP_RECVD,
  CLIENT_STATE_ERROR
} urob_http_client_state;

typedef struct
{
  struct netconn * conn; //TODO: no uninit/destroy
  err_t err;
  urob_http_client_state state;
  ip_addr_t address;
  int port;

  //message handling section
  char * message;
  int msg_len;
  size_t msg_written;
} urob_http_client;

void urob_http_client_init(urob_http_client * client, ip_addr_t * address, int port);
void urob_http_client_uninit(urob_http_client * client);
void urob_http_client_loop(urob_http_client * client);

#endif //__UROB_HTTP_CLIENT_H__