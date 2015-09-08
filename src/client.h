#ifndef CLIENT_H_
#define CLIENT_H_

typedef struct _write_queue_el_t { 
	STAILQ_ENTRY(_write_queue_el_t) next;
	time_t time;	// sent time
	char data[];
} write_queue_el_t;

STAILQ_HEAD(_write_queue_t, _write_queue_el_t);
typedef struct _write_queue_t write_queue_t;

typedef struct _client_t {
	uv_tcp_t handle;
	int started;
	write_queue_t write_queue;	// I apdu
	char ctl_apdu[6];			// U or S apdu
	int queue_len;
	struct {
		unsigned char  res1:1;
		unsigned short ns:15;
		unsigned char  res2:1;
		unsigned short nr:15;
		int nk;	// number of not confirmed
	};
	uv_timer_t t1;	// подтвержд. последнего переданного
	uv_timer_t t1_u;// подтвержд. последнего переданного U
	uv_timer_t t2;	// подтвержд. последнего принятого	
	uv_timer_t t3;	// тестирование при неактивности 
	uv_timer_t tc;	// cyclic
} client_t;


extern int client_parse_request(client_t *clt, const uv_buf_t *buf, ssize_t sz);
extern void enqueue(client_t *clt, char *data, int size);
extern void response(client_t *clt);
extern client_t *client_create();
extern void client_close(uv_handle_t *h);
extern void start_t1(client_t *clt);
extern void restart_t1(client_t *clt, int s);
extern void start_t2(client_t *clt);
extern void start_t3(client_t *clt);
extern void start_tc(client_t *clt);
#endif
