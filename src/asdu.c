#define _GNU_SOURCE
#include <sys/queue.h>
#include <uv.h>


#include "client.h"
#include "apdu.h"
#include "asdu.h"
#include "log.h"

// Поддерживаемые команды управления
// 1. Общий опрос C_IC_NA_1
// 2. Опрос счетчиков 

// Cause of transmission
#define COT_CYCLIC 1
#define COT_BACKGROUND_SCAN 2
#define COT_SPONTANEOUS 3
#define COT_INITIALIZED 4
#define COT_REQUEST 5
#define COT_ACTIVATION 6
#define COT_ACTIVATION_CONFIRMATION 7
#define COT_DEACTIVATION 8
#define COT_DEACTIVATION_CONFIRMATION 9
#define COT_ACTIVATION_TERMINATION 10
#define COT_REMOTE_COMMAND 11
#define COT_LOCAL_COMMAND 12
#define COT_FILE_TRANSFER 13
#define COT_STATION_INTERROGATION 20
#define COT_GROUP_INTERROGATION 21
#define COT_GENERAL_COUNTER_REQUEST 37
#define COT_GROUP_COUNTER_REQUEST 38
#define COT_UNKNOWN_TYPE_IDENTIFICATION 44
#define COT_UNKNOWN_CAUSE_OF_TRANSMISSION 45
#define COT_UNKNOWN_COMMON_ADDRESS 46
#define COT_UNKNOWN_INFORMATION_OBJECT_ADDRESS 47


void process_asdu(client_t *clt, asdu_t *in_asdu, int size)
{
	char buf[APDU_MAX_LEN+2];
	int offset = 0;
	int out_apdu_size;
	apdu_t *out_apdu = (apdu_t *)buf;
	asdu_t *out_asdu = (asdu_t *)&buf[sizeof(apdu_t)];
	out_asdu->dui.type_id = in_asdu->dui.type_id;

	unsigned short cot;
	switch (in_asdu->dui.type_id) {
		case C_IC_NA_1: {
			init_apdu(clt, buf, AT_I);
				
			break;
		}
		default: {
			cot = COT_UNKNOWN_TYPE_IDENTIFICATION;
			goto err;
		}
	}


	return;
err:
	
}
