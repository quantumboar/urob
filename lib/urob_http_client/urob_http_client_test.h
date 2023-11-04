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

#ifndef __UROB_HTTP_CLIENT_TEST_H__
#define __UROB_HTTP_CLIENT_TEST_H__

#include "urob_http_client.h"
#include "urob_address.h"

typedef enum
{
    HTTP_CLIENT_TEST_STATE_NONE,
    HTTP_CLIENT_TEST_STATE_ERROR,
    HTTP_CLIENT_TEST_STATE_RESOLVING_ADDRESS,
    HTTP_CLIENT_TEST_STATE_WAITING_RESPONSE,
    HTTP_CLIENT_TEST_STATE_RESPONSE_RECEIVED
} urob_http_client_test_state;

typedef struct
{
    urob_address address;
    urob_http_client http_client;
    urob_http_client_test_state state;
} urob_http_client_test;

void urob_http_client_test_init(urob_http_client_test * http_client_test);
void urob_http_client_test_uninit(urob_http_client_test * http_client_test);
void urob_http_client_test_loop(urob_http_client_test * http_client_test);

#endif //__UROB_HTTP_CLIENT_TEST_H__