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

#include "urob_http_client.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "lwip/opt.h"
#include "lwip/arch.h"
#include "lwip/api.h"

#include "esp_log.h"

#define TAG "http client"
#include "general.h"

static char header_format_string[] = "GET / HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n";

void _urob_http_client_callback(struct netconn *conn, enum netconn_evt evt, u16_t len)
{
    ESP_LOGD(TAG, "Connection event: %d", evt);
}

void urob_http_client_init(urob_http_client * client, ip_addr_t * address, int port)
{
    client->address = * address;
    client->port = port;
    client->conn = netconn_new_with_callback(NETCONN_TCP, _urob_http_client_callback);
    _chk(client->conn == NULL, client->err = ERR_CONN, "Unable to initialize client");

    netconn_set_flags(client->conn, NETCONN_FLAG_NON_BLOCKING);
    client->state = CLIENT_STATE_INIT;
}

// Assumes the client is initialized (no additional checks)
static void _urob_http_client_connect(urob_http_client * client)
{
    ESP_LOGI(TAG, "connecting to host %s:%d", ipaddr_ntoa(&client->address), client->port);
    client->err = netconn_connect(client->conn, &client->address, client->port);
    if (client->err == ERR_INPROGRESS || client->err == ERR_ALREADY || client->err == ERR_OK)
    {
        ESP_LOGD(TAG, "connection in progress.."); // may need to throttle this log
        client->err = ERR_OK;
        client->state = CLIENT_STATE_CONNECTING;
    } else
    {
        ESP_LOGE(TAG, "error connecting: %d", client->err);
        client->state = CLIENT_STATE_ERROR;
    }
}

static void _urob_http_client_connecting(urob_http_client * client)
{
    if (client->conn->state == NETCONN_NONE) {
        ESP_LOGI(TAG, "connected to host");
        client->state = CLIENT_STATE_CONNECTED;
    } else if (client->conn->state == NETCONN_CLOSE) { //Not sure this is the right state after a connection timeout, should check the value from the callback instead
        ESP_LOGE(TAG, "connection closed");
        client->state = CLIENT_STATE_ERROR;
    } else {
         ESP_LOGD(TAG, "connection state: %d", client->conn->state);
    }
}

static void _urob_http_client_set_message(urob_http_client * client, char * format, ...)
{
    if (client->message)
    {
        ESP_LOGW(TAG, "client already has message, replacing");
        free(client->message);
    }

    va_list vars;
    va_start(vars, format);
    client->msg_len = vasprintf(&client->message, format, vars);
    va_end(vars);

    client->msg_written = 0;
}

// Assumes the client is connected (no additional checks)
static void _urob_http_client_send_request(urob_http_client * client)
{
    if (client->message == NULL) // create request
    {
        _urob_http_client_set_message(client, header_format_string, "ipwho.is");
        _chk(client->msg_len == -1, client->state = CLIENT_STATE_ERROR, "Error creating message");
    }

    ESP_LOGI(TAG, "sending request");
    size_t bytes_written = 0;
    // Don't copy, don't block
    client->err = netconn_write_partly(
        client->conn, client->message, client->msg_len + client->msg_written, NETCONN_DONTBLOCK, &bytes_written);
    _chk(client->err != ERR_OK && client->err != ERR_INPROGRESS, client->state = CLIENT_STATE_ERROR, "error sending request: %d", client->err);

    client->msg_written += bytes_written;
    ESP_LOGD(TAG, "%d/%d bytes sent", client->msg_written, client->msg_len);

    if (client->msg_written == client->msg_len)
    {
        ESP_LOGI(TAG, "request sent, waiting for response");
        client->state = CLIENT_STATE_WAIT_RESP;
    }
}

// Assumes the request was sent
static void _urob_http_client_recv_response(urob_http_client * client)
{
    struct pbuf * recv_pbuf = NULL;
    client->err = netconn_recv_tcp_pbuf_flags(client->conn, &recv_pbuf, NETCONN_DONTBLOCK);

    if (client->err == ERR_WOULDBLOCK || client->err == ERR_INPROGRESS)
    {
        client->err = ERR_OK;
    }

    _chk(client->err != ERR_OK, return, "error receiving response: %d", client->err);

    if (recv_pbuf != NULL)
    {
        //TODO: this would be the perfect place for a callback to send to the client
        ESP_LOGD(TAG, "Received data: len:%d tot_len: %d <%.*s>",
            recv_pbuf->len, recv_pbuf->tot_len, recv_pbuf->len, (char *)recv_pbuf->payload);

        pbuf_free(recv_pbuf);

        // TODO: how to detect the message terminated?
        ESP_LOGI(TAG, "message received");
        client->state = CLIENT_STATE_RESP_RECVD;
    }
}

void urob_http_client_loop(urob_http_client * client)
{
    switch (client->state)
    {
        case CLIENT_STATE_NONE:
            return;
        break;
        case CLIENT_STATE_ERROR:
            ESP_LOGE(TAG, "error state");
            urob_http_client_uninit(client);
        break;
        case CLIENT_STATE_INIT:
            _urob_http_client_connect(client);
        break;
        case CLIENT_STATE_CONNECTING:
            _urob_http_client_connecting(client);
        break;
        case CLIENT_STATE_CONNECTED:
            _urob_http_client_send_request(client);
        break;
        case CLIENT_STATE_WAIT_RESP:
            _urob_http_client_recv_response(client);
        break;
        case CLIENT_STATE_RESP_RECVD:
            //urob_http_client_uninit(client);
        break;
        default:
        ESP_LOGE(TAG, "Unhandled http client state: %d", client->state);
        client->err = ERR_VAL;
    }

    if (client->err != ERR_OK && client->err != ERR_INPROGRESS)
    {
        ESP_LOGE(TAG, "netconn error %d", client->err);
        client->state = CLIENT_STATE_ERROR;
    }
}


void urob_http_client_uninit(urob_http_client * client)
{
    ESP_LOGI(TAG, "uninitializing client");

    if (client->state >= CLIENT_STATE_CONNECTED)
    {
        netconn_disconnect(client->conn);
    }

    if (client->message)
    {
        free(client->message);
    }

    if (client->conn)
    {
        netconn_delete(client->conn);
    }
    *client = (urob_http_client) {0};
}

