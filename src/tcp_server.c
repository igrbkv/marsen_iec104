#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <uv.h>

#include "iec104.h"
#include "log.h"
#include "client.h"

#include "debug.h"

#define DEFAULT_BACKLOG 128


extern uv_loop_t *loop;
static struct sockaddr_in addr;
static uv_tcp_t server;

void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
	buf->base = (char*) malloc(suggested_size);
	buf->len = suggested_size;
#ifdef DEBUG
	iec104_log(LOG_DEBUG, "alloc to read: %lu", suggested_size);
#endif
}


void tcp_server_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {
	if (nread < 0) {
		if (nread != UV_EOF)
			iec104_log(LOG_ERR, "Read error %s\n", uv_err_name(nread));
#ifdef DEBUG
		iec104_log(LOG_DEBUG, "Connection %p closed: %s", client, uv_err_name(nread));
#endif
		uv_close((uv_handle_t *) client, client_close);
	} else if (nread > 0) {
		if (client_parse_request((client_t *)client, buf, nread) == -1)
			iec104_log(LOG_ERR, "Ошибка разбора пакета");
	}
	if (buf->base) {
#ifdef DEBUG
		iec104_log(LOG_DEBUG, "free to read");
#endif
		free(buf->base);
	}
}

void on_new_connection(uv_stream_t *server, int status) {
	if (status < 0) {
		iec104_log(LOG_ERR, "New connection error %s\n", uv_strerror(status));
		// error!
		return;
	}

	client_t *c = client_create();
#ifdef DEBUG
	iec104_log(LOG_DEBUG, "New connection: %p", c);
#endif
	uv_tcp_init(loop, (uv_tcp_t *)c);
	if (uv_accept(server, (uv_stream_t*) c) == 0) {
		uv_read_start((uv_stream_t*) c, alloc_buffer, tcp_server_read);
	} else {
		uv_close((uv_handle_t*) c, client_close);
	}
}

int tcp_server_init()
{
	uv_tcp_init(loop, &server);

	uv_ip4_addr("0.0.0.0", IEC104_PORT, &addr);

	uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0);
	int r = uv_listen((uv_stream_t*) &server, DEFAULT_BACKLOG, on_new_connection);
	if (r) {
		iec104_log(LOG_ERR, "Listen error %s\n", uv_strerror(r));
		return -1;
	}
	return 0;
}

int tcp_server_close()
{
	return 0;
}
