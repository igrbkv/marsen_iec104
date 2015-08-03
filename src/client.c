#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/queue.h>
#include <uv.h>

#include "iec104.h"
#include "write_queue.h"
#include "client.h"
#include "log.h"

#include "debug.h"

#define APDU_START 0x68
#define APDU_MAX_LEN 253
#define APDU_MAX_COUNT 32767

// Parse states
typedef enum {
	AT_I,
	AT_S,
	AT_U
} APDU_TYPE;

typedef struct _apci_t {
	char start_byte;
	unsigned char len;
	union {
		struct f_i {
			unsigned char  ft:1;
			unsigned short ns:15;
			unsigned char  res:1;
			unsigned short nr:15;
			} i;
		struct f_s {
			unsigned char  ft:1;
			unsigned short res1:15;
			unsigned char  res2:1;
			unsigned short nr:15;
		} s;
		struct f_u {
			unsigned char  ft1:1;
			unsigned char  ft2:1;
			unsigned char  startdt_act:1;
			unsigned char  startdt_con:1;
			unsigned char  stopdt_act:1;
			unsigned char  stopdt_con:1;
			unsigned char  testfr_act:1;
			unsigned char  testfr_con:1;
			unsigned char  res1;
			unsigned short res2;
		} u;
	};
} apci_t;

typedef struct {
} asdu_t;

typedef struct {
	apci_t apci;
	asdu_t asdu[];
} apdu_t;

typedef struct _write_req_t {
	uv_write_t req;
	uv_buf_t buf;
} write_req_t;

unsigned short iec104_k;
unsigned short iec104_w;

void print_apdu(const char *head, const char *buf, int size)
{
	char *p = NULL;
	asprintf(&p, "%s: ", head);
	for (int i = 0; i < size; i++)
		asprintf(&p, "%s %02x", p, buf[i]);
	iec104_log(LOG_DEBUG, "%s", p);
	free(p);
}


APDU_TYPE apdu_type(apdu_t *apdu)
{
	if (apdu->apci.i.ft == 0)
		return AT_I;
	else if (apdu->apci.u.ft1 && apdu->apci.u.ft2)
		return AT_U;

	return AT_S;
}

void client_set_started(client *c)
{
	c->started = 1;
}

void client_set_stopped(client *c)
{
	c->started = 0;
}

void on_write(uv_write_t *req, int status)
{
	write_req_t *wr = (write_req_t*) req;

	free(wr->buf.base);
	free(wr);
}


int check(client *clt, uv_buf_t *buf, int offset)
{
	apdu_t *apdu = (apdu_t *)&buf->base[offset];
	char *err_msg = NULL;
	// header check
	if (apdu->apci.start_byte != APDU_START)
		asprintf(&err_msg, "APDU: invalid start byte: %02x", apdu->apci.start_byte);
	else if (buf->len-offset < sizeof(apci_t))
		asprintf(&err_msg, "APDU: buffer is too small: %ld bytes", buf->len-offset);
	else if (apdu->apci.len > APDU_MAX_LEN ||
		apdu->apci.len < sizeof(apci_t)-2)
		asprintf(&err_msg, "APDU: invalid length: %d", apdu->apci.len);
	else if (apdu->apci.len > buf->len-offset)
		asprintf(&err_msg, "APDU: buffer bound violation: apdu length: %d buffer length: %ld", apdu->apci.len, buf->len-offset);
	
	if (err_msg) {
		print_apdu("rcv", (const char *)apdu, buf->len);
		goto err;
	}
	
	APDU_TYPE type = apdu_type(apdu);
	// ptotocol check
	if ((type == AT_U || type == AT_S)
		&& apdu->apci.len != sizeof(apci_t)-2)
		asprintf(&err_msg, "APDU: invalid length for non information format: %d", apdu->apci.len);
	else if (type == AT_U &&
		// FIXME bit test => mask test
		(((apdu->apci.u.res1 != 0 || 
		apdu->apci.u.res2 != 0)) ||
		(apdu->apci.u.startdt_act && 
		(apdu->apci.u.startdt_con ||
		apdu->apci.u.stopdt_act ||
		apdu->apci.u.stopdt_con ||
		apdu->apci.u.testfr_act ||
		apdu->apci.u.testfr_con)) ||
		(apdu->apci.u.stopdt_act && 
		(apdu->apci.u.startdt_act ||
		apdu->apci.u.startdt_con ||
		apdu->apci.u.stopdt_con ||
		apdu->apci.u.testfr_act ||
		apdu->apci.u.testfr_con)) ||
		(apdu->apci.u.testfr_act && 
		(apdu->apci.u.startdt_act ||
		apdu->apci.u.startdt_con ||
		apdu->apci.u.stopdt_act ||
		apdu->apci.u.stopdt_con ||
		apdu->apci.u.testfr_con))))
		asprintf(&err_msg, "APDU: invalid U format");
	else if (type == AT_U && 
		(apdu->apci.u.startdt_con ||
		apdu->apci.u.stopdt_con ||
		apdu->apci.u.testfr_con))
		asprintf(&err_msg, "APDU: unexpected %s", apdu->apci.u.startdt_con? "STARTDT_con": apdu->apci.u.stopdt_con? "STOPDT_con": "TESTFR_con");
	else if (type == AT_I &&
		apdu->apci.i.res)
		asprintf(&err_msg, "APDU: invalid I format");
	else if (type == AT_S &&
		apdu->apci.s.res2)
		asprintf(&err_msg, "APDU: invalid S format");
	else if (clt->started) {
		switch (type) {
			case AT_U:
				if (apdu->apci.u.startdt_act)
					asprintf(&err_msg, "APDU: unexpected STARTDT_act");
				break;
			case AT_I:
				// Проверка числа переданных сервером APDU
				if (apdu->apci.i.ns != clt->count.nr) {
					asprintf(&err_msg, "APDU: invalid sent num: %d expected: %d", apdu->apci.i.ns, clt->count.nr);
					break;
				}

			case AT_S: {
				// Проверка числа подтвержденных сервером APDU
				int kv = ((int)clt->count.ns) - clt->nk;
				if (kv < 0) kv += APDU_MAX_COUNT; 
				if ((kv > clt->count.ns &&
					apdu->apci.s.nr > clt->count.ns &&
					apdu->apci.s.nr < kv) ||
					(kv <= clt->count.ns &&
					(apdu->apci.s.nr < kv || 
					apdu->apci.s.nr > clt->count.ns)))
					asprintf(&err_msg, "APDU: invalid confirmed number: %d last send: %d wait to confirm: %d", apdu->apci.s.nr, clt->count.ns, clt->nk);
				break;
			}
		}
	}

	if (err_msg) {
		print_apdu("APDU", (const char *)apdu, buf->len);
		goto err;
	}
	return 0;
err:
	iec104_log(LOG_DEBUG,"%s", err_msg);
	free(err_msg);
	return -1;
}

void process_request(client *clt, apdu_t *apdu)
{
	APDU_TYPE type = apdu_type(apdu);
	switch (type) {
		case AT_U: {
			if (apdu->apci.u.startdt_act) {
				apdu_t *resp = malloc(sizeof(apdu_t));
				memset(resp, 0, sizeof(apdu_t));
				resp->apci.u.startdt_con = 1;
				write_queue_append(clt, resp);
			}
		}
	}
}

void response(client *clt)
{
}

int client_parse_request(client *clt, uv_buf_t *buf)
{
	int offset = 0;
	apdu_t *apdu;
	while (offset < buf->len) {
		if (check(clt, buf, offset) == -1) {
			uv_close((uv_handle_t*) clt, client_close);
			return -1;
		}
		
		apdu = (apdu_t *)&buf->base[offset];
		process_request(clt, apdu);

		offset += apdu->apci.len + 2;
	}

	response(clt);

	return 0;
}

client *client_create()
{
	client *c = (client *)malloc(sizeof(client));
	uv_tcp_init(loop, (uv_tcp_t *)c);
	uv_timer_init(loop, &c->t1);
	uv_timer_init(loop, &c->t2);
	uv_timer_init(loop, &c->t3);
	write_queue_init(&c->write_queue);
	return c;
}

void client_close(uv_handle_t *h)
{
	client *c = (client *)h;
#ifdef DEBUG
	iec104_log(LOG_DEBUG, "Client %p destroyed", h);
#endif
	uv_timer_stop(&c->t1);
	uv_timer_stop(&c->t2);
	uv_timer_stop(&c->t3);
	write_queue_destroy(&c->write_queue);

	free(h);
}
