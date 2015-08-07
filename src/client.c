#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/queue.h>
#include <uv.h>

#include "iec104.h"
#include "client.h"
#include "apdu.h"
#include "log.h"

#include "debug.h"


typedef struct _write_req_t {
	uv_write_t req;
	uv_buf_t buf;
} write_req_t;

unsigned short iec104_k;
unsigned short iec104_w;

static APDU_TYPE apdu_type(apdu_t *apdu);
void process_request(client_t *clt, apdu_t *apdu);
void remove_confirmed(client *clt)
static void response(client *clt);





void on_write(uv_write_t *req, int status)
{
	write_req_t *wr = (write_req_t*) req;

	free(wr->buf.base);
	free(wr);

	if (status < 0) {
		uv_close((uv_handle_t*)req->handle, client_close);
		return;
	}
	
	// FIXME сделать мультипакетную передачу и убрать
	// отсюда
	response();
}




// удалить из очереди подтвержденные APDU
void remove_confirmed(client *clt)
{
	write_queue_el_t *v, tmp;
	STAILQ_FOREACH(v, &clt->write_queue, next) {
		tmp = *v;
		apdu_t *apdu = (apdu_t *)v->data;
		if (apdu_type(apdu) == AT_I) {
			if (apdu->apci.i.ns == clt->count.ns)
				break;
			STAILQ_REMOVE(&clt->write_queue, v, write_queue_t, next);
			free(apdu);
			free(v);
			v = &tmp;
		}
	}
}

void process_i(client_t *clt, apdu_t *apdu)
{
	process_asdu(clt, &apdu->asdu[0], apdu->apci.len+2-sizeof(apci_t));
}

void process_request(client_t *clt, apdu_t *apdu)
{
	APDU_TYPE type = apdu_type(apdu);
	switch (type) {
		case AT_U: {
			if (apdu->apci.u.startdt_act) {
				clt->started = 1;
				apdu_t *resp = malloc(sizeof(apdu_t));
				init_apdu(clt, (char *)resp, AT_U);
				resp->apci.u.startdt_con = 1;
				write_queue_el_t *el = (write_queue_el_t *)malloc(sizeof(*el));
				el->apdu = resp;
				STAILQ_INSERT_TAIL(&clt->write_queue, el, next);
			}
			break;	
		}
		case AT_S: {
			clt->count.ns = apdu->apci.s.nr;
			remove_confirmed(clt);
			break;
		}
		case AT_I: {
			clt->count.ns = apdu->apci.i.nr;
			remove_confirmed(clt);
			process_i(clt, apdu);
			break;
		}

	}
}

void response(client_t *clt)
{
	write_queue_el_t *v;
	apdu_t *apdu;
	void *buf = NULL;
	size_t len;

	STAILQ_FOREACH(v, &clt->write_queue, next) {
		apdu = (apdu_t *) v->apdu;
		APDU_TYPE type = apdu_type(apdu); 
		if (type == AT_U || type == AT_S) {
			buf = apdu;
			len = apdu->apci.len+2;
			// информационные APDU остаются в очереди 
			// до подтверждения
			STAILQ_REMOVE(&clt->write_queue, v, _write_queue_el_t, next);
			free(v);
			break;
		}
	}

	if (buf) {
		write_req_t* wr = (write_req_t *)malloc(sizeof(*wr));
		wr->buf = uv_buf_init(buf, len);
#ifdef DEBUG
		print_apdu("write", buf, len);
#endif
		int r = uv_write(&wr->req, (uv_stream_t *)clt, &wr->buf, 1, on_write);
		if (r != 0) {
			iec104_log(LOG_DEBUG, "uv_write failed: %s", uv_strerror(r));
			uv_close((uv_handle_t *)clt, client_close);
		}
	}
}

int client_parse_request(client_t *clt, const uv_buf_t *buf, ssize_t sz)
{
	int offset = 0;
	apdu_t *apdu;
	while (offset < sz) {
		if (check_apdu(clt, buf, sz, offset) == -1) {
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

client_t *client_create()
{
	client_t *c = (client_t *)malloc(sizeof(client_t));
	memset(c, 0, sizeof(*c));
	uv_tcp_init(loop, (uv_tcp_t *)c);
	uv_timer_init(loop, &c->t1);
	uv_timer_init(loop, &c->t2);
	uv_timer_init(loop, &c->t3);

	STAILQ_INIT(&c->write_queue);
	return c;
}

void client_close(uv_handle_t *h)
{
	client_t *c = (client_t *)h;
#ifdef DEBUG
	iec104_log(LOG_DEBUG, "Client %p destroyed", h);
#endif
	uv_timer_stop(&c->t1);
	uv_timer_stop(&c->t2);
	uv_timer_stop(&c->t3);

	// очистить очередь
	write_queue_t *q = &c->write_queue;
	while (!STAILQ_EMPTY(q)) {
		write_queue_el_t *fe = STAILQ_FIRST(q);
		STAILQ_REMOVE_HEAD(q, next);
		free(fe->data);
		free(fe);
	}

	free(h);
}
