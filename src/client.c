#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/queue.h>
#include <uv.h>

#include "iec104.h"
#include "client.h"
#include "apdu.h"
#include "asdu.h"
#include "log.h"

#include "debug.h"


typedef struct _write_req_t {
	uv_write_t req;
	uv_buf_t buf;
} write_req_t;

int iec104_k = 12; // max num of sent APDU waiting for confirm
int iec104_w = 8; // -- " --   received  -- " --

static void process_request(client_t *clt, apdu_t *apdu);
static void remove_confirmed(client_t *clt, unsigned short n);
static void response(client_t *clt);


void on_write(uv_write_t *req, int status)
{
	write_req_t *wr = (write_req_t*) req;

	free(wr->buf.base);
	free(wr);

	if (status < 0) {
		uv_close((uv_handle_t*)req->handle, client_close);
		return;
	}
}


// удалить из очереди подтвержденные APDU
void remove_confirmed(client_t *clt, unsigned short n)
{
	write_queue_el_t *el;

	struct {
		unsigned char  res1:1;
		unsigned short n:15;
	} n2r;
	n2r.n = clt->ns - n;
	
	while (n2r.n) {
		el = STAILQ_FIRST(&clt->write_queue);
		STAILQ_REMOVE_HEAD(&clt->write_queue, next);
		free(el);
		clt->nk--;
		clt->queue_len--;
		n2r.n--;
	}
}

void process_i(client_t *clt, apdu_t *apdu)
{
	process_asdu(clt, (asdu_t *)apdu->asdu, apdu->apci.len+2-sizeof(apci_t));
}

void enqueue(client_t *clt, char *data, int size)
{
	write_queue_el_t *el = (write_queue_el_t *)malloc(sizeof(*el)+size);
	memcpy(el->data, data, size);
	STAILQ_INSERT_TAIL(&clt->write_queue, el, next);

	clt->queue_len++;
}

void process_request(client_t *clt, apdu_t *apdu)
{
	APDU_TYPE type = apdu_type(apdu);
	switch (type) {
		case AT_U: {
			apdu_t *resp = (apdu_t *)clt->ctl_apdu;
			init_apdu(clt, resp, AT_U);
			if (apdu->apci.u.startdt_act) {
				clt->started = 1;
				resp->apci.u.startdt_con = 1;

			} else if (apdu->apci.u.testfr_act) {
				resp->apci.u.testfr_con = 1;
			}
			break;	
		}
		case AT_S: {
			remove_confirmed(clt, apdu->apci.s.nr);
			break;
		}
		case AT_I: {
			clt->nr++;
			remove_confirmed(clt, apdu->apci.i.nr);
			process_i(clt, apdu);
			break;
		}

	}
}

void response(client_t *clt)
{
	write_queue_el_t *el;
	apdu_t *apdu;
	char *buf = NULL;
	size_t buf_len = 0, apdu_len;
	int nk = 0;

	STAILQ_FOREACH(el, &clt->write_queue, next) {
		if (nk < clt->nk) {
			nk++;
			continue;
		}
		else if (nk == iec104_k)
			break;
		clt->nk++;
		apdu = (apdu_t *)&el->data[0];
		apdu_len = apdu->apci.len+2;
		buf = realloc(buf, buf_len + apdu_len);
		memcpy(&buf[buf_len], apdu, apdu_len);
		buf_len += apdu_len;
	}
	
	// add U/S apdu
	apdu = (apdu_t *)clt->ctl_apdu;
	apdu_len = apdu->apci.len+2;
	if (apdu->apci.len) {
		buf = realloc(buf, buf_len + apdu_len);
		memcpy(&buf[buf_len], apdu, apdu_len);
		buf_len += apdu_len;
		apdu->apci.len = 0;
	}

	if (!buf)
		return;

	write_req_t* wr = (write_req_t *)malloc(sizeof(*wr));
	wr->buf = uv_buf_init(buf, buf_len);
#ifdef DEBUG
	print_apdu("write", buf, buf_len);
#endif
	int r = uv_write(&wr->req, (uv_stream_t *)clt, &wr->buf, 1, on_write);
	if (r != 0) {
		iec104_log(LOG_DEBUG, "uv_write failed: %s", uv_strerror(r));
		uv_close((uv_handle_t *)clt, client_close);
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
		free(fe);
	}

	free(h);
}
