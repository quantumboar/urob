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

#ifndef __UROB_ADDRESS_H__
#define __UROB_ADDRESS_H__

#include "lwip/err.h"
#include "lwip/ip_addr.h"
#include <stdatomic.h>

typedef enum
{
  ADDRESS_STATE_NONE = 0,
  ADDRESS_STATE_ERROR,
  ADDRESS_STATE_INIT,
  ADDRESS_STATE_RESOLVING,
  ADDRESS_STATE_RESOLVED,
} urob_address_state;

typedef struct 
{
  ip_addr_t address;
  err_t err;
  atomic_int state;
} urob_address;

void urob_address_init(urob_address * address, const char * dnsname);
void urob_address_uninit(urob_address * address);
inline bool netconn_address_resolved(urob_address * address) { return atomic_load_explicit(&address->state, memory_order_acquire) == ADDRESS_STATE_RESOLVED; }

#endif //__UROB_ADDRESS_H__
