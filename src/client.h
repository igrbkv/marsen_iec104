#ifndef CLIENT_H_
#define CLIENT_H_

typedef struct client {
	uv_tcp_t handle;
	int started;
	write_queue_t write_queue;
	struct {
		unsigned char  res1:1;
		unsigned short ns:15;
		unsigned char  res2:1;
		unsigned short nr:15;
	} count;
	int nk;	// число ожидающих квитирования
	uv_timer_t t1;
	uv_timer_t t2;
	uv_timer_t t3;
} client;

typedef void (*write_handler)(client *handle);

extern int client_parse_request();
extern client *client_create();
extern void client_close(uv_handle_t *h);
#endif
