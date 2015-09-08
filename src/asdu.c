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
// 2. Активация/деактивация P_AC_NA_1 


unsigned short iec104_station_address;

int check_asdu(client_t *clt, asdu_t *in_asdu, int size)
{
	unsigned char err_cot = 0;

	switch (in_asdu->dui.type_id) {
		case C_IC_NA_1: {
			if (in_asdu->dui.code != COT_ACTIVATION &&
				in_asdu->dui.code != COT_DEACTIVATION) {
				err_cot = COT_UNKNOWN_CAUSE_OF_TRANSMISSION;
				break;
			}
			break;
		}
		case P_AC_NA_1: {
			if (in_asdu->dui.code != COT_ACTIVATION &&
				in_asdu->dui.code != COT_DEACTIVATION) {
				err_cot = COT_UNKNOWN_CAUSE_OF_TRANSMISSION;
				break;
			}
			unsigned short adr = in_asdu->inf_obj[0].adr;
			if (in_asdu->inf_obj[0].not_used_adr_byte ||				!adr_exist(adr)) {
				err_cot = COT_UNKNOWN_INFORMATION_OBJECT_ADDRESS;
				break;
			}
			break;
		}
		case C_RD_NA_1: {
			if (in_asdu->dui.code != COT_REQUEST) {
				err_cot = COT_UNKNOWN_CAUSE_OF_TRANSMISSION;
				break;
			}
			unsigned short adr = in_asdu->inf_obj[0].adr;
			if (in_asdu->inf_obj[0].not_used_adr_byte ||				!adr_exist(adr)) {
				err_cot = COT_UNKNOWN_INFORMATION_OBJECT_ADDRESS;
				break;
			}
			break;
		}
		case C_CS_NA_1: {
			if (in_asdu->dui.code != COT_ACTIVATION) {
				err_cot = COT_UNKNOWN_CAUSE_OF_TRANSMISSION;
				break;
			}
			unsigned short adr = in_asdu->inf_obj[0].adr;
			if (in_asdu->inf_obj[0].not_used_adr_byte ||
				adr) {
				err_cot = COT_UNKNOWN_INFORMATION_OBJECT_ADDRESS;
				break;
			}
			break;
		}
		default: {
			err_cot = COT_UNKNOWN_TYPE_IDENTIFICATION;
			break;
		}
	}

	if (!err_cot && in_asdu->dui.common_adr) 
		err_cot = COT_UNKNOWN_COMMON_ADDRESS;

	if (err_cot) {
		char buf[APDU_MAX_LEN+2] = {0};
		apdu_t *out_apdu = (apdu_t *)buf;
		asdu_t *out_asdu = (asdu_t *)&buf[sizeof(apdu_t)];
		init_apdu(clt, out_apdu, AT_I);
		memcpy(out_asdu, in_asdu, size );
		out_apdu->apci.len += size;
		out_asdu->dui.code = err_cot; 
		enqueue_apdu(clt, out_apdu);
		return -1;
	}
	return 0;
}

void process_asdu(client_t *clt, asdu_t *in_asdu, int size)
{
	char buf[APDU_MAX_LEN+2] = {0};
	apdu_t *out_apdu = (apdu_t *)buf;
	asdu_t *out_asdu = (asdu_t *)&buf[sizeof(apdu_t)];

	switch (in_asdu->dui.type_id) {
		case C_IC_NA_1: {
			if (in_asdu->dui.code == COT_ACTIVATION) {
				// confirmation
				init_apdu(clt, out_apdu, AT_I);
				*out_asdu = *in_asdu;
				out_asdu->dui.code = COT_ACTIVATION_CONFIRMATION;
				out_asdu->dui.originator_adr = iec104_station_address;

				out_asdu->inf_obj[0].inf_el[0].t100.qual.QOI = 20;
				out_apdu->apci.len += sizeof(asdu_t) + sizeof(inf_obj_t) + sizeof(qualifier_t);
				enqueue_apdu(clt, out_apdu);

				// data transmission
				station_interrogation(clt, 0);
				
				// termination
				init_apdu(clt, out_apdu, AT_I);
				*out_asdu = *in_asdu;
				out_asdu->dui.code = COT_ACTIVATION_TERMINATION;
				out_apdu->apci.len += sizeof(asdu_t) + sizeof(inf_obj_t) + sizeof(qualifier_t);
				enqueue_apdu(clt, out_apdu);
			} else { // COT_DEACTIVATION
				init_apdu(clt, out_apdu, AT_I);
				*out_asdu = *in_asdu;
				out_asdu->dui.code = COT_DEACTIVATION_CONFIRMATION;
				out_asdu->dui.originator_adr = iec104_station_address;
				out_apdu->apci.len += sizeof(asdu_t) + sizeof(inf_obj_t) + sizeof(qualifier_t);
				enqueue_apdu(clt, out_apdu);
			}
			break;
		}
		case P_AC_NA_1: {
			unsigned short adr = in_asdu->inf_obj[0].adr;
			activate_analog(adr, 
				in_asdu->dui.code == COT_ACTIVATION?1: 0);
			init_apdu(clt, out_apdu, AT_I);
			memcpy(out_asdu, in_asdu, size);
			out_apdu->apci.len += size;
			// COT_X + 1 = COT_X_CONFIRMATION
			out_asdu->dui.code = in_asdu->dui.code + 1;
			out_asdu->dui.originator_adr = iec104_station_address;
			enqueue_apdu(clt, out_apdu);
			break;
		}
		case C_RD_NA_1: {
			unsigned short adr = in_asdu->inf_obj[0].adr;
			read_single_data(clt, adr);
			break;
		}
		case C_CS_NA_1: {
			init_apdu(clt, out_apdu, AT_I);
			memcpy(out_asdu, in_asdu, size);
			out_apdu->apci.len += size;
			out_asdu->dui.originator_adr = iec104_station_address;
			CP56Time2a_t *time56 = 
				&out_asdu->inf_obj[0].inf_el[0].t103.time;

			sync_clock(time56);
			enqueue_apdu(clt, out_apdu);
			break;		
		}
	}
}
