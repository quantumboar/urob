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

#include "urob_http_client_test.h"
#include <esp_log.h>

#define TAG "http client test"
#include "general.h"

void urob_http_client_test_init(urob_http_client_test * http_client_test)
{
    * http_client_test = (urob_http_client_test) {0};
    urob_address_init(&http_client_test->address, "ipwho.is");
    http_client_test->state = HTTP_CLIENT_TEST_STATE_RESOLVING_ADDRESS;
}

void urob_http_client_test_uninit(urob_http_client_test * http_client_test)
{
    ESP_LOGI(TAG, "uninitializing");
    urob_address_uninit(&http_client_test->address);
    urob_http_client_uninit(&http_client_test->http_client);
    * http_client_test = (urob_http_client_test) {0};
}

static void _netconn_http_client_resolving_address(urob_http_client_test * http_client_test)
{
    if (! netconn_address_resolved(&http_client_test->address))
    {
        return;
    }

    ESP_LOGI(TAG, "address resolved");
    urob_http_client_init(&http_client_test->http_client, &http_client_test->address.address, 80);
    http_client_test->state = HTTP_CLIENT_TEST_STATE_WAITING_RESPONSE;
}

void urob_http_client_test_loop(urob_http_client_test * http_client_test)
{
    switch(http_client_test->state)
    {
        case HTTP_CLIENT_TEST_STATE_ERROR:
            ESP_LOGE(TAG, "error state");
            urob_http_client_test_uninit(http_client_test);
        break;
        case HTTP_CLIENT_TEST_STATE_NONE:
            return;
        break;
        case HTTP_CLIENT_TEST_STATE_RESOLVING_ADDRESS:
            _netconn_http_client_resolving_address(http_client_test);
        break;
        case HTTP_CLIENT_TEST_STATE_WAITING_RESPONSE:
            urob_http_client_loop(&http_client_test->http_client);
        break;
        default:
            ESP_LOGE(TAG, "unhandled state: %d", http_client_test->state);
    }
}