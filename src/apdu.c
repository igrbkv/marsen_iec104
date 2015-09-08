#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/queue.h>
#include <uv.h>

#include "client.h"
#include "apdu.h"
#include "asdu.h"
#include "log.h"

#include "debug.h"

#define APDU_START 0x68

void print_apdu(const char *head, const char *buf, int size)
{
	char *p = NULL;
	int total = 0, j = 0;
	asprintf(&p, "%s (%d)[0]:", head, size);
	while (total < size) {
		apdu_t *apdu = (apdu_t *)&buf[total];
		for (int i = 0; i < apdu->apci.len+2; i++, total++)
			asprintf(&p, "%s %02x", p, (unsigned char)buf[total]);
		iec104_log(LOG_DEBUG, "%s", p);
		asprintf(&p, "%s[%d]:", head, ++j);
	}
	free(p);
}

int check_apdu(client_t *clt, const uv_buf_t *buf, ssize_t sz, int offset)
{
	// FIXME добавить проверку ASDU
	// ret = 0, если возможен ответ клиенту 
	// об ошибке в ASDU, а не разрыв связи
	int ret = -1;

	apdu_t *apdu = (apdu_t *)&buf->base[offset];
	char *err_msg = NULL;
	// header check
	if (apdu->apci.start_byte != APDU_START)
		asprintf(&err_msg, "APDU: invalid start byte: %02x", apdu->apci.start_byte);
	else if (sz-offset < sizeof(apci_t))
		asprintf(&err_msg, "APDU: msg is too small: %ld bytes", sz-offset);
	else if (apdu->apci.len > APDU_MAX_LEN ||
		apdu->apci.len < sizeof(apci_t)-2)
		asprintf(&err_msg, "APDU: invalid length: %d", apdu->apci.len);
	else if (apdu->apci.len > sz-offset)
		asprintf(&err_msg, "APDU: buffer bound violation: apdu length: %d buffer length: %ld", apdu->apci.len, sz-offset);
	
	if (err_msg) {
		print_apdu("rcv", (const char *)apdu, sz);
		goto err;
	}

#ifdef DEBUG
	print_apdu("rcv", (const char *)apdu, sz);
#endif
	
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
		apdu->apci.u.stopdt_con))
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
				// Проверка числа переданных клиентом APDU
				if (apdu->apci.i.ns != clt->nr) {
					asprintf(&err_msg, "APDU: invalid apdu received num: %u expected: %u", apdu->apci.i.ns, clt->nr);
					break;
				}

			case AT_S: {
				// Проверка числа принятых клиентом APDU
				struct {
					unsigned char res1:1;
					unsigned short n:15;
				} nh, nc;

				nh.n = clt->ns - clt->queue_len;
				nc.n = apdu->apci.i.nr - nh.n;
				if (nc.n > clt->nk) {
					asprintf(&err_msg, "APDU: invalid client receive num: %u server queueded num first:%u last:%u", apdu->apci.i.nr, nh.n, clt->ns);
					break;
				}
			}
		}
	}

	if (err_msg) {
		print_apdu("APDU:", (const char *)apdu, sz);
		goto err;
	}
	return 0;
err:
	iec104_log(LOG_DEBUG,"%s", err_msg);
	free(err_msg);
	return ret;
}

void enqueue_apdu(client_t *clt, apdu_t *apdu)
{
	enqueue(clt, (char *)apdu, apdu->apci.len+2);
}

void init_apdu(client_t *clt, apdu_t *apdu, APDU_TYPE type)
{
	*apdu = (const apdu_t){0};
	apdu->apci.start_byte = APDU_START;
	apdu->apci.len = sizeof(apdu_t)-2; 
	switch (type) {
		case AT_S: {
			apdu->apci.s.ft = 1;
			break;
		}
		case AT_U: {
	            apdu->apci.u.ft1 = 1;
				apdu->apci.u.ft2 = 1;

			break;
		}
		case AT_I: {
			apdu->apci.i.ns = clt->ns++;
			apdu->apci.i.nr = clt->nr;
			break;
		}
	}
}
