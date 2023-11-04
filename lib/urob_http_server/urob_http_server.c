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

#include "urob_http_server.h"
#include "string.h"
#include "lwip/err.h"
#include "lwip/api.h"

#include "esp_log.h"

#define TAG "http server"
#include "general.h"

static char http_header[] = "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n";
static char html_page[] = "<html><head><title>Test server</title></head><body><h1>Urob(oron)</h1><p>Welcome to Urob(oron)'s http server!</p></body></html>";

void urob_http_server_init(urob_http_server *server)
{
#if LWIP_IPV6
    server->conn = netconn_new(NETCONN_TCP_IPV6);
    server->err = netconn_bind(server->conn, IP6_ADDR_ANY, 80);
#else  /* LWIP_IPV6 */
    server->conn = netconn_new(NETCONN_TCP);
    server->err = netconn_bind(server->conn, IP_ADDR_ANY, 80);
#endif /* LWIP_IPV6 */
    netconn_set_recvtimeout(server->conn, 5);
    _chk(server->conn == NULL, return, "Unable to setup connection");
    server->err = netconn_listen(server->conn);
    _chk(server->err != ESP_OK, , "error while listening: %d", server->err);
}

void urob_http_server_uninit(urob_http_server * server)
{
  ESP_LOGI(TAG, "Uninitializing");
  err_t err = netconn_close(server->conn);
  _chk(err != ERR_OK, , "netconn_close: %d", err);
  err = netconn_delete(server->conn);
  _chk(err != ERR_OK, , "netconn_delete: %d", err);
  * server = (urob_http_server) {0};
}

// TODO: make non-blocking
static void _urob_http_server_serve(urob_http_server * server, struct netconn *new_conn)
{
    struct netbuf * new_buf;
    server->err = netconn_recv(new_conn, &new_buf);
    _chk(server->err != ERR_OK, goto leave, "error receiving: %d", server->err);

    char *buf;
    u16_t len;
    server->err = netbuf_data(new_buf, (void **)&buf, &len);
    _chk(server->err != ERR_OK, goto leave, "netbuf_data: %d", server->err);

    if (strncmp(buf, "GET /", 5) == 0)
    {
        // TODO: these write functions should use NETCONN_DONTBLOCK
        server->err = netconn_write(new_conn, http_header, sizeof(http_header) - 1, NETCONN_NOCOPY);
        _chk(server->err != ERR_OK, goto leave, "sending header: %d", server->err);
        server->err = netconn_write(new_conn, html_page, sizeof(html_page) - 1, NETCONN_NOCOPY);
        _chk(server->err != ERR_OK, goto leave, "sending html: %d", server->err);
    }

leave:
    server->err = netconn_close(new_conn);
    netbuf_delete(new_buf);
}

void urob_http_server_loop(urob_http_server * server)
{
  _chk(server->err != ERR_OK, urob_http_server_uninit(server), "server error: %d", server->err);

  struct netconn *newconn;
  server->err = netconn_accept(server->conn, &newconn);

  if (server->err == ERR_OK)
  {
      ESP_LOGI(TAG, "received connection request");
      _urob_http_server_serve(server, newconn);
      netconn_delete(newconn);
  }

  if (server->err == ERR_TIMEOUT)
  {
      server->err = ERR_OK;
  }
}