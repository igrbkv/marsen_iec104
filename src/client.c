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

int iec104_k; // max num of sent APDU waiting for confirm
int iec104_w; // -- " --   received  -- " --

static void process_request(client_t *clt, apdu_t *apdu);
static void remove_confirmed(client_t *clt, unsigned short n);


void on_write(uv_write_t *req, int status)
{
	uv_handle_t *h = (uv_handle_t *)req->handle;
	write_req_t *wr = (write_req_t*) req;

	free(wr->buf.base);
	free(wr);

	if (status < 0) {
		uv_close(h, client_close);
		return;
	}
}

// удалить из очереди подтвержденные APDU
void remove_confirmed(client_t *clt, unsigned short nr)
{
	write_queue_el_t *el;
#if 0
	struct {
		unsigned char  res1:1;
		unsigned short n:15;
	} n2r;
	// номер первого
	n2r.n = clt->ns - clt->queue_len;
	n2r.n = nr - n2r.n;
	
	while (n2r.n) {
		el = STAILQ_FIRST(&clt->write_queue);
		STAILQ_REMOVE_HEAD(&clt->write_queue, next);
		free(el);
		clt->nk--;
		clt->queue_len--;
		n2r.n--;
	}
#else
	// num not confirmed
	int nc = clt->ns - nr;
	if (nc < 0)
		nc += APDU_MAX_COUNT;
	// num confirmed
	nc = clt->queue_len - nc;
	while (nc) {
		el = STAILQ_FIRST(&clt->write_queue);
		STAILQ_REMOVE_HEAD(&clt->write_queue, next);
		free(el);
		clt->nk--;
		clt->queue_len--;
		nc--;
	}
#endif

	if (clt->nk == 0) {
		uv_timer_stop(&clt->t1);
#ifdef DEBUG
		iec104_log(LOG_DEBUG, "stop t1");
#endif
	} else if (!STAILQ_EMPTY(&clt->write_queue)) {
		el = STAILQ_FIRST(&clt->write_queue);		
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC_COARSE, &ts);
		restart_t1(clt, ts.tv_sec - el->time);
	}
}

void process_i(client_t *clt, apdu_t *apdu)
{
	if (check_asdu(clt, (asdu_t *)apdu->asdu, apdu->apci.len+2-sizeof(apci_t)) != -1)
		process_asdu(clt, (asdu_t *)apdu->asdu, apdu->apci.len+2-sizeof(apci_t));
}

void enqueue(client_t *clt, char *data, int size)
{
	write_queue_el_t *el = (write_queue_el_t *)malloc(sizeof(*el)+size);
	el->time = 0;
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
			if (apdu->apci.u.startdt_act) {
				init_apdu(clt, resp, AT_U);
				clt->started = 1;
				resp->apci.u.startdt_con = 1;
				// cyclic
				start_tc(clt);
			} else if (apdu->apci.u.stopdt_act) {
				init_apdu(clt, resp, AT_U);
				resp->apci.u.stopdt_con = 1;
			} else if (apdu->apci.u.testfr_act) {
				init_apdu(clt, resp, AT_U);
				resp->apci.u.testfr_con = 1;
			} else if (apdu->apci.u.testfr_con) {
				uv_timer_stop(&clt->t1_u);
#ifdef DEBUG
				iec104_log(LOG_DEBUG, "stop t1_u");
#endif
			}

			break;	
		}
		case AT_S: {
			remove_confirmed(clt, apdu->apci.s.nr);
			break;
		}
		case AT_I: {
			if (!uv_is_active((uv_handle_t *)&clt->t2))
				start_t2(clt);

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
	int last_nk = clt->nk;
	int nk = 0;
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC_COARSE, &ts);

	STAILQ_FOREACH(el, &clt->write_queue, next) {
		if (nk++ < clt->nk) {
			continue;
		}
		else if (nk > iec104_k)
			break;
		clt->nk++;
		apdu = (apdu_t *)&el->data[0];
		apdu_len = apdu->apci.len+2;
		buf = realloc(buf, buf_len + apdu_len);
		memcpy(&buf[buf_len], apdu, apdu_len);
		buf_len += apdu_len;
		el->time = ts.tv_sec;
	}

	// add U/S apdu
	apdu = (apdu_t *)clt->ctl_apdu;
	if (apdu->apci.len) {
		apdu_len = apdu->apci.len+2;	
		buf = realloc(buf, buf_len + apdu_len);
		memcpy(&buf[buf_len], apdu, apdu_len);
		buf_len += apdu_len;
		apdu->apci.len = 0;
	}

	if (!buf)
		return;
	
	if (last_nk != clt->nk) { 
		// если посылаются AT_I
		// t1 запустить 
		if (!uv_is_active((uv_handle_t *)&clt->t1))
			start_t1(clt);

		// t2 остановить
		if (uv_is_active((uv_handle_t *)&clt->t2)) {
			uv_timer_stop(&clt->t2);
#ifdef DEBUG
			iec104_log(LOG_DEBUG, "stop t2");
#endif
		}
	}

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

	start_t3(clt);

	while (offset < sz) {
		if (check_apdu(clt, buf, sz, offset) == -1) {
			uv_close((uv_handle_t*) clt, client_close);
			return -1;
		}
		
		apdu = (apdu_t *)&buf->base[offset];
		process_request(clt, apdu);

		response(clt);

		offset += apdu->apci.len + 2;
	}
	return 0;
}

client_t *client_create()
{
	client_t *c = (client_t *)malloc(sizeof(client_t));
	memset(c, 0, sizeof(*c));
	uv_tcp_init(loop, (uv_tcp_t *)c);
	c->t1.data = c;
	c->t1_u.data = c;
	c->t2.data = c;
	c->t3.data = c;
	c->tc.data = c;
	uv_timer_init(loop, &c->t1);
	uv_timer_init(loop, &c->t1_u);
	uv_timer_init(loop, &c->t2);
	uv_timer_init(loop, &c->t3);
	uv_timer_init(loop, &c->tc);

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
	uv_timer_stop(&c->t1_u);
	uv_timer_stop(&c->t2);
	uv_timer_stop(&c->t3);
	uv_timer_stop(&c->tc);

	// очистить очередь
	write_queue_t *q = &c->write_queue;
	while (!STAILQ_EMPTY(q)) {
		write_queue_el_t *fe = STAILQ_FIRST(q);
		STAILQ_REMOVE_HEAD(q, next);
		free(fe);
	}

	free(h);
}
