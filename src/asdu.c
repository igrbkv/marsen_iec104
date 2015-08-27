#define _GNU_SOURCE
#include <string.h>
#include <sys/queue.h>
#include <uv.h>


#include "client.h"
#include "apdu.h"
#include "asdu.h"
#include "log.h"

#include "debug.h"

// Поддерживаемые команды управления
// 1. Общий опрос C_IC_NA_1
// 2. Опрос счетчиков 


unsigned char iec104_originator_adr;

int process_asdu(client_t *clt, asdu_t *in_asdu, int size)
{
	char buf[APDU_MAX_LEN+2];
	apdu_t *out_apdu = (apdu_t *)buf;
	asdu_t *out_asdu = (asdu_t *)&buf[sizeof(apdu_t)];

	switch (in_asdu->dui.type_id) {
		case C_IC_NA_1: {
			// confirmation
			init_apdu(clt, out_apdu, AT_I);
			*out_asdu = *in_asdu;
			out_asdu->dui.code = COT_ACTIVATION_CONFIRMATION;
			out_asdu->inf_obj[0] = (const inf_obj_t){0};
			out_asdu->inf_obj[0].inf_el[0].t100.qual.QOI = 20;
			out_apdu->apci.len += sizeof(asdu_t) + sizeof(inf_obj_t) + sizeof(qualifier_t);
			enqueue_apdu(clt, out_apdu);

			// data transmission
			if (station_interrogation(clt, 0, in_asdu->dui.originator_adr) == -1)
				return -1;
			
			// termination
			init_apdu(clt, out_apdu, AT_I);
			*out_asdu = *in_asdu;
			out_asdu->dui.code = COT_ACTIVATION_TERMINATION;
			out_apdu->apci.len += sizeof(asdu_t) + sizeof(inf_obj_t) + sizeof(qualifier_t);
			enqueue_apdu(clt, out_apdu);
			break;
		}
		default: {
			init_apdu(clt, out_apdu, AT_I);
			memcpy(out_asdu, in_asdu, size );
			out_apdu->apci.len += size;
			out_asdu->dui.code = COT_UNKNOWN_TYPE_IDENTIFICATION;
			enqueue_apdu(clt, out_apdu);
		}
	}
	return 0;
}
