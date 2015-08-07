#ifndef CLIENT_H_
#define CLIENT_H_

typedef struct _write_queue_el_t { 
	STAILQ_ENTRY(_write_queue_el_t) next;
	void *apdu;
} write_queue_el_t;

STAILQ_HEAD(_write_queue_t, _write_queue_el_t);
typedef struct _write_queue_t write_queue_t;

typedef struct _client_t {
	uv_tcp_t handle;
	int started;
	write_queue_t write_queue;
	struct {
		unsigned char  res1:1;
		unsigned short ns:15;
		unsigned char  res2:1;
		unsigned short nr:15;
	};
	uv_timer_t t1;
	uv_timer_t t2;
	uv_timer_t t3;
} client_t;


extern int client_parse_request(client_t *clt, const uv_buf_t *buf, ssize_t sz);
extern client_t *client_create();
extern void client_close(uv_handle_t *h);
#endif
