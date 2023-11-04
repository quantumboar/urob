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

#include "urob_address.h"
#include "lwip/dns.h"
#include <esp_log.h>
#define TAG "address"

#include "general.h"

static void _urob_address_dns_found(const char *name, const ip_addr_t *resolved_address, void *arg)
{
  LWIP_UNUSED_ARG(arg);
  urob_address * address = (urob_address *) arg;

  ESP_LOGI(TAG, "%s: %s\n", name, address ? ipaddr_ntoa(resolved_address) : "<not found>");

  if (resolved_address == NULL)
  {
    address->err = ERR_ARG;
  } else
  {
    address->address = *resolved_address;
  }

  atomic_store_explicit(&address->state, ADDRESS_STATE_RESOLVED, memory_order_release);
}

void urob_address_init(urob_address * address, const char * dnsname)
{
  * address = (urob_address){0};
  atomic_store_explicit(&address->state, ADDRESS_STATE_RESOLVING, memory_order_release);

  address->err = dns_gethostbyname(dnsname, &address->address, _urob_address_dns_found, address);
  if (address->err == ERR_OK)
  {
    _urob_address_dns_found(dnsname, &address->address, address);
  }
  _chk(address->err != ERR_INPROGRESS, address->state = ADDRESS_STATE_ERROR, "error resolving address: %d", address->err);

  ESP_LOGI(TAG, "resolving address..");
}

void urob_address_uninit(urob_address * address)
{
  * address = (urob_address) {0};
}

