#include "urob_tcp.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "lwip/opt.h"
#include "lwip/arch.h"
#include "lwip/api.h"

#define TAG "tcp message"
#include "general.h"

void urob_tcp_message_init(urob_tcp_message * tcp_message, urob_tcp_message_type type)
{
    * tcp_message = (urob_tcp_message) {0};

    tcp_message->type = type;
    tcp_message->state = UROB_TCP_MESSAGE_STATE_INIT;
}

void urob_tcp_message_payload_printf(urob_tcp_message * tcp_message, const char * format, ...)
{
    if (tcp_message->payload)
    {
        ESP_LOGW(TAG, "tcp_message already has payload, replacing");
        free(tcp_message->payload);
    }

    va_list vars;
    va_start(vars, format);
    tcp_message->length = vasprintf(&tcp_message->payload, format, vars);
    va_end(vars);

    tcp_message->free_payload_in_uninit = true;
}

void urob_tcp_message_uninit(urob_tcp_message * tcp_message)
{
    if (tcp_message->type == UROB_TCP_MESSAGE_TYPE_OUTGOING &&
        tcp_message->free_payload_in_uninit &&
        tcp_message->payload != NULL)
    {
        free(tcp_message->payload);
    }

    if (tcp_message->type == UROB_TCP_MESSAGE_TYPE_INCOMING &&
        tcp_message->head_pbuf != NULL)
    {
        // Will free all pbufs in a chain
        pbuf_free(tcp_message->head_pbuf);
    }

    * tcp_message = (urob_tcp_message) {0};
}

void _urob_tcp_callback(struct netconn *conn, enum netconn_evt evt, u16_t len)
{
    ESP_LOGD(TAG, "Connection event: %d", evt);
}

// TODO: harmonize with listening (accept) connections
void urob_tcp_init_client(urob_tcp * tcp, ip_addr_t * address, int port)
{
    * tcp = (urob_tcp) {0};
    tcp->type = UROB_TCP_TYPE_CLIENT;

    tcp->address = * address;
    tcp->port = port;
    tcp->conn = netconn_new_with_callback(NETCONN_TCP, _urob_tcp_callback);
    _chk(tcp->conn == NULL, tcp->err = ERR_CONN, "unable to initialize");

    netconn_set_flags(tcp->conn, NETCONN_FLAG_NON_BLOCKING);
    tcp->state = UROB_TCP_STATE_INIT;
}

void urob_tcp_init_server(urob_tcp * tcp, int port)
{
    * tcp = (urob_tcp) {0};
    tcp->type = UROB_TCP_TYPE_SERVER;

    #if LWIP_IPV6
    tcp->conn = netconn_new(NETCONN_TCP_IPV6);
    tcp->err = netconn_bind(tcp->conn, IP6_ADDR_ANY, port);
#else  /* LWIP_IPV6 */
    tcp->conn = netconn_new(NETCONN_TCP);
    tcp->err = netconn_bind(tcp->conn, IP_ADDR_ANY, port);
#endif /* LWIP_IPV6 */
    netconn_set_recvtimeout(tcp->conn, 5);
    _chk(tcp->conn == NULL, return, "Unable to setup connection");
    tcp->err = netconn_listen(tcp->conn);
    _chk(tcp->err != ESP_OK, , "error while listening: %d", tcp->err);

    //netconn_set_flags(tcp->conn, NETCONN_FLAG_NON_BLOCKING);
    tcp->state = UROB_TCP_STATE_INIT;
}

// Assumes the tcp is initialized (no additional checks)
static void _urob_tcp_connect(urob_tcp * tcp)
{
    ESP_LOGI(TAG, "connecting to host %s:%d", ipaddr_ntoa(&tcp->address), tcp->port);
    tcp->err = netconn_connect(tcp->conn, &tcp->address, tcp->port);
    if (tcp->err == ERR_INPROGRESS || tcp->err == ERR_ALREADY || tcp->err == ERR_OK)
    {
        ESP_LOGD(TAG, "connection in progress.."); // may need to throttle this log
        tcp->err = ERR_OK;
        tcp->state = UROB_TCP_STATE_CONNECTING;
    } else
    {
        ESP_LOGE(TAG, "error connecting: %d", tcp->err);
        tcp->state = UROB_TCP_STATE_ERROR;
    }
}

static void _urob_tcp_connecting(urob_tcp * tcp)
{
    if (tcp->conn->state == NETCONN_NONE) {
        ESP_LOGI(TAG, "connected to host");
        tcp->state = UROB_TCP_STATE_CONNECTED;
    } else if (tcp->conn->state == NETCONN_CLOSE) { //Not sure this is the right state after a connection timeout, should check the value from the callback instead
        ESP_LOGE(TAG, "connection closed");
        tcp->state = UROB_TCP_STATE_ERROR;
    } else {
        ESP_LOGD(TAG, "connection state: %d", tcp->conn->state);
    }
}

void urob_tcp_add_message(urob_tcp * tcp, urob_tcp_message * tcp_message)
{
    int message_index = 0;

    for (; message_index < MAX_UROB_TCP_MESSAGES; message_index ++)
    {
        if (tcp->messages[message_index] == NULL)
        {
            tcp->messages[message_index] = tcp_message;
            return;
        }
    }

    ESP_LOGE(TAG, "message limit reached, unable to add");
    tcp_message->err = ERR_MEM;
}

static void _urob_tcp_remove_message(urob_tcp * tcp, urob_tcp_message * tcp_message)
{
    int message_index = 0;

    for (; message_index < MAX_UROB_TCP_MESSAGES; message_index ++)
    {
        if (tcp->messages[message_index] == tcp_message)
        {
            tcp->messages[message_index] = NULL;
            return;
        }
    }

    ESP_LOGE(TAG, "message not found in messages");
}

static void _urob_tcp_service_messages(urob_tcp * tcp)
{
    int message_index = 0;

    for (; message_index < MAX_UROB_TCP_MESSAGES; message_index ++)
    {
        urob_tcp_message * tcp_message = tcp->messages[message_index];
        if ( tcp_message != NULL)
        {
            switch(tcp_message->type)
            {
                case UROB_TCP_MESSAGE_TYPE_INCOMING:
                    _urob_tcp_receive_message(tcp, tcp_message);
                break;
                case UROB_TCP_MESSAGE_TYPE_OUTGOING:
                    _urob_tcp_send_message(tcp, tcp_message);
                break;
                default:
                    ESP_LOGE(TAG, "unrecognized message type, removing");
                    _urob_tcp_remove_message(tcp, tcp_message);
            }

            if (tcp_message->err != ERR_OK)
            {
                ESP_LOGE(TAG, "error in message, removing");
                _urob_tcp_remove_message(tcp, tcp_message);
            }
        }
    }
}

// Assumes the tcp is connected (no additional checks)
static void _urob_tcp_send_message(urob_tcp * tcp, urob_tcp_message * tcp_message)
{
    switch (tcp_message->state)
    {
        case UROB_TCP_MESSAGE_STATE_INIT:
            ESP_LOGD(TAG, "sending message");
            tcp_message->state = UROB_TCP_MESSAGE_STATE_SENDING;
        break;
        case UROB_TCP_MESSAGE_STATE_SENDING:
            // all ok here
        break;
        default:
            ESP_LOGE("message in wrong state for sending :%d", tcp_message->state);
            tcp_message->state = UROB_TCP_MESSAGE_STATE_ERROR;
            return;
    }

    size_t bytes_written = 0; // bytes written in this iteration

    tcp_message->err = netconn_write_partly(
        tcp->conn,
        tcp_message->payload,
        tcp_message->length + tcp_message->progress,
        NETCONN_DONTBLOCK, // Don't copy, don't block
        &bytes_written);

    _chk(tcp_message->err != ERR_OK && tcp_message->err != ERR_INPROGRESS, tcp_message->state = UROB_TCP_MESSAGE_STATE_ERROR, "error sending request: %d", tcp_message->err);

    tcp_message->progress += bytes_written;
    ESP_LOGD(TAG, "%d/%d bytes sent", tcp_message->progress, tcp_message->length);

    if (tcp_message->progress == tcp_message->length)
    {
        ESP_LOGI(TAG, "message sent");
        tcp_message->state = UROB_TCP_MESSAGE_STATE_SENT;
        _urob_tcp_remove_message(tcp, tcp_message);
    }
}

// TODO: must register a callback to serve specific requests..
static void _urob_tcp_serve(urob_tcp * tcp, struct netconn *new_conn)
{
    struct netbuf * new_buf;
    tcp->err = netconn_recv(new_conn, &new_buf);
    _chk(tcp->err != ERR_OK, goto leave, "error receiving: %d", tcp->err);

    char *buf;
    u16_t len;
    tcp->err = netbuf_data(new_buf, (void **)&buf, &len);
    _chk(tcp->err != ERR_OK, goto leave, "netbuf_data: %d", tcp->err);

    if (strncmp(buf, "GET /", 5) == 0)
    {
        // TODO: these write functions should use NETCONN_DONTBLOCK
        tcp->err = netconn_write(new_conn, http_header, sizeof(http_header) - 1, NETCONN_NOCOPY);
        _chk(tcp->err != ERR_OK, goto leave, "sending header: %d", tcp->err);
        tcp->err = netconn_write(new_conn, html_page, sizeof(html_page) - 1, NETCONN_NOCOPY);
        _chk(tcp->err != ERR_OK, goto leave, "sending html: %d", tcp->err);
    }

leave:
    server->err = netconn_close(new_conn);
    netbuf_delete(new_buf);
}


static void _urob_tcp_accept(urob_tcp * tcp)
{
  struct netconn *newconn;
  tcp->err = netconn_accept(tcp->conn, &newconn);

  if (tcp->err == ERR_OK)
  {
      ESP_LOGI(TAG, "received connection request");
      _urob_tcp_serve(tcp, newconn);
      netconn_delete(newconn);
  }

  if (tcp->err == ERR_TIMEOUT)
  {
      tcp->err = ERR_OK;
  }
}

// Assumes the request was sent
static void _urob_tcp_receive_message(urob_tcp * tcp, urob_tcp_message * tcp_message)
{
    switch (tcp_message->state)
    {
        case UROB_TCP_MESSAGE_STATE_INIT:
            ESP_LOGD(TAG, "receiving message");
            tcp_message->state = UROB_TCP_MESSAGE_STATE_RECEIVING;
        break;
        case UROB_TCP_MESSAGE_STATE_RECEIVING:
            // all ok here
        break;
        default:
            ESP_LOGE("message in wrong state for receiving :%d", tcp_message->state);
            tcp_message->state = UROB_TCP_MESSAGE_STATE_ERROR;
            return;
    }

    struct pbuf * tail_pbuf = NULL;
    tcp_message->err = netconn_recv_tcp_pbuf_flags(tcp->conn, &tail_pbuf, NETCONN_DONTBLOCK);

    if (tcp_message->err == ERR_WOULDBLOCK || tcp_message->err == ERR_INPROGRESS)
    {
        tcp_message->err = ERR_OK;
    }

    // Apparently we need to keep polling until we get an error (see lwip_recv_tcp() in sockets.c)
    if (tcp_message->err != ERR_OK) {
        tcp_message->state = UROB_TCP_MESSAGE_STATE_RECEIVED;
        _urob_tcp_remove_message(tcp, tcp_message);
        ESP_LOGD(TAG, "done receiving message, code: %d", tcp_message->err);
        return;
    }

    if (tail_pbuf != NULL)
    {
        ESP_LOGD(TAG, "Received payload: len:%d tot_len: %d <%.*s>",
            tail_pbuf->len,
            tail_pbuf->tot_len,
            tail_pbuf->len,
            (char *)tail_pbuf->payload); 

        if (tcp_message->head_pbuf != NULL)
        {
            pbuf_cat(tcp_message->head_pbuf, tail_pbuf); // takes ownership of tail_pbuf
        }
        else {
            tcp_message->head_pbuf = tail_pbuf;
        }
    }
}

void urob_tcp_loop(urob_tcp * tcp)
{
    switch (tcp->state)
    {
        case UROB_TCP_STATE_NONE:
            return;
        break;
        case UROB_TCP_STATE_ERROR:
            ESP_LOGE(TAG, "error state");
            urob_tcp_uninit(tcp);
        break;
        case UROB_TCP_STATE_INIT:
            if (tcp->type == UROB_TCP_TYPE_CLIENT)
            {
                _urob_tcp_connect(tcp);
            } else if (tcp->type == UROB_TCP_TYPE_SERVER)
            {
                tcp->state = UROB_TCP_STATE_ACCEPTING;
            } else
            {
                ESP_ERR(TAG, "unkown tcp type: %d", tcp->type);
                tcp->state = UROB_TCP_STATE_ERROR;
            }
        break;
        case UROB_TCP_STATE_CONNECTING:
            _urob_tcp_connecting(tcp);
        break;
        case UROB_TCP_STATE_CONNECTED:
            _urob_tcp_service_messages(tcp);
        break;
        case UROB_TCP_STATE_ACCEPTING:
            _urob_tcp_accept();
        break;
        default:
        ESP_LOGE(TAG, "unhandled state: %d", tcp->state);
        tcp->err = ERR_VAL;
    }

    if (tcp->err != ERR_OK && tcp->err != ERR_INPROGRESS)
    {
        ESP_LOGE(TAG, "netconn error %d", tcp->err);
        tcp->state = UROB_TCP_STATE_ERROR;
    }
}


void urob_tcp_uninit(urob_tcp * tcp)
{
    ESP_LOGI(TAG, "uninitializing tcp");
    err_t err = ERR_OK;

    _chk(tcp->conn != NULL, goto leave, "no connection");

    if (tcp->type == UROB_TCP_TYPE_CLIENT && tcp->state >= UROB_TCP_STATE_CONNECTED)
    {
        err = netconn_disconnect(tcp->conn);
        _chk(err != ERR_OK, , "netconn_disconnect: %d", err);
    }

    if (tcp->type == UROB_TCP_TYPE_SERVER)
    {
        err = netconn_close(tcp->conn);
        _chk(err != ERR_OK, , "netconn_close: %d", err);
    }

    err = netconn_delete(tcp->conn);
    _chk(err != ERR_OK, , "netconn_delete: %d", err);

leave:
    *tcp = (urob_tcp) {0};
}