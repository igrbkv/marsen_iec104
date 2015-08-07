#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <uv.h>

#include "client.h"
#include "apdu.h"
#include "asdu.h"
#include "log.h"

#define APDU_START 0x68
#define APDU_MAX_LEN 253
#define APDU_MAX_COUNT 32767

static void print_apdu(const char *head, const char *buf, int size);

void print_apdu(const char *head, const char *buf, int size)
{
	char *p = NULL;
	asprintf(&p, "%s ", head);
	for (int i = 0; i < size; i++)
		asprintf(&p, "%s %02x", p, buf[i]);
	iec104_log(LOG_DEBUG, "%s", p);
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
		print_apdu("rcv:", (const char *)apdu, sz);
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
		print_apdu("APDU:", (const char *)apdu, sz);
		goto err;
	}
	return 0;
err:
	iec104_log(LOG_DEBUG,"%s", err_msg);
	free(err_msg);
	return ret;
}

void init_apdu(client *clt, char *buf, APDU_TYPE apdu_type)
{
	apdu_t *apdu = (apdu_t *)buf;
	*apdu = {0};
	apdu->apci.start_byte = APDU_START;
	apdu->apci.len = sizeof(apdu_t)-2; 
	switch (apdu_type) {
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
