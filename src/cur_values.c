#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <medb.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <uv.h>

#include "iec104.h"
#include "client.h"
#include "apdu.h"
#include "asdu.h"
#include "log.h"

#include "debug.h"

/* 1. Измеряемые величины.
 * Все - тип R32.
 * Диапазон адресов 1 - 12623.
 * Размещение данных соответственно 180 - 50592. 
 * Все принадлежат одной группе №1.
 * 
 * 2. Счетчики.
 * Один параметр - абсолютный индекс.
 * Диапазон адресов 30000 - 30001
 * Абсолютный индекс. Адрес 30000.
 * Группа счетчиков №1.
 *
 * 3. Параметры по умолчанию, участвующие в общем 
 * опросе,  опросе групп и циклической/периодической 
 * передаче 
 * Группа 1. Измеряемые величины. *******************
 * Смещение Адрес	Кол.	Тип		Параметр
 * 180		1		1		R32		Средняя частота
 * 424		62		10		R32		Напряжения ДЗ
 * 464		72		10		R32		Напряжения средние
 * 504		82		10		R32		Напр. средневыпрям.
 * 544		92		4		R32		Токи ДЗ
 * 560		96		4		R32		Токи средние
 * 576		100		4		R32		Токи средневыпрям.
 * 592		104		16		R32		Активная мощность
 * 656		120		16		R32		Реактивная мощность
 * 50512	12584	10		R32		Фликер напр.мгн.макс
 * 50552	12594	10		R32		Флк. напр.кратк.доза
 * Группа 1. Счетчики. ******************************
 * Смещение Адрес	Кол.	Тип		Параметр
 * 0		30000	1		BCR		Абсолютный индекс
 *
 * 4. Циклическая передача.
 * Передаются аналоги без метки времени.
 *
 * TODO
 *	- Спорадическая передача. В идеале: Редкая 
 *	циклическая передача и спорадическая по изменению
 *	- Установка времени
 *	- Передача файлов
 */

int iec104_analogs_offset;
int iec104_dsp_data_size;
// Строка из конф. файла вида
// 1, 62-71, ...
char *iec104_periodic_analogs;


static int *periodic_analogs = NULL;
static int periodic_analogs_num;
static void *dsp_data = NULL;
static int dsp_data_invalid;
static int check_periodic_analogs();
static void list_periodic_analogs();
static float get_value_by_adr(int adr);

int cur_values_init()
{
	int ret = -1;

	periodic_analogs_num = check_periodic_analogs();
	if (periodic_analogs_num == -1)
		goto err;

	dsp_data = malloc(iec104_dsp_data_size);
	list_periodic_analogs();

	ret = 0;
err:
	free(iec104_periodic_analogs);
	iec104_periodic_analogs = NULL;
	return ret;
}

int cur_values_close()
{
	free(iec104_periodic_analogs);
	iec104_periodic_analogs = NULL;
	free(dsp_data);
	dsp_data = NULL;
	return 0;
}


// список д.б. возрастающий
// @return: число аналоговых сигналов или -1
int check_periodic_analogs()
{
	char *comma_token, *dash_token;
	int order = 0;
	char *str = strdup(iec104_periodic_analogs);
	int ret = -1;
	int num = 0;
	char *err_str = NULL;

	// Parse
	while (comma_token = strsep(&str, ",")) {
		int lb,rb;
		dash_token = strsep(&comma_token, "-");
		lb = atoi(dash_token);
		if (lb <= order) {
			err_str = "Адреса сигналов должны быть перечислены в возрастающем порядке";
			goto err;
		}
		if (comma_token) {
			rb = atoi(comma_token);
			if (rb <= lb) {
				err_str = "Неверный диапазон адресов сигналов";
				goto err;
			}
			order = rb;
			num += (rb - lb) + 1;
		} else {
			order = lb;
			num++;
		}
	}
	if (order > (iec104_dsp_data_size-iec104_analogs_offset)/(sizeof(float))) {
		err_str = "Несуществующие адреса сигналов";
		goto err;
	}
	ret = num;
err:
	if (err_str)
		iec104_log(LOG_ERR, "%s: %s", err_str, iec104_periodic_analogs);
	free(str);
	return ret;
}

int adr_exist(unsigned short adr)
{
	if (adr == 0 || adr > (iec104_dsp_data_size-iec104_analogs_offset)/(sizeof(float)))
		return 0;
	return 1;
}

// активация/деактивация аналога на время сеанса
int activate_analog(unsigned short adr, int new_state)
{
	// is adr active?
	int active = 0, i; 
	for (i = 0; i < periodic_analogs_num; i++)
		if (periodic_analogs[i] >= adr) {
			active = periodic_analogs[i] == adr;
			break;
		}
	// nothing to do
	if (active == new_state)
		return 0;

	periodic_analogs_num += (new_state? 1: -1);
	periodic_analogs = (int *)realloc(periodic_analogs, periodic_analogs_num);
	if (new_state) {
		memcpy(&periodic_analogs[i+1], &periodic_analogs[i], (periodic_analogs_num-i-1)*sizeof(int));
		periodic_analogs[i] = adr;
	} else
		memcpy(&periodic_analogs[i], &periodic_analogs[i+1], (periodic_analogs_num-i)*sizeof(int));
	return 0;
}

void list_periodic_analogs()
{	
	char *comma_token, *dash_token;
	char *str = strdup(iec104_periodic_analogs);
	int i = 0;
	periodic_analogs = (int *)calloc(periodic_analogs_num, sizeof(int));

	while (comma_token = strsep(&str, ",")) {
		int lb,rb;
		dash_token = strsep(&comma_token, "-");
		lb = atoi(dash_token);
		if (comma_token) {
			rb = atoi(comma_token);
		} else
			rb = lb;

		while (lb<=rb)
			periodic_analogs[i++] = lb++;
	}

	free(str);
}

void read_dsp_data(client_t *clt)
{
	dsp_data_invalid = 1;
	int sz = getRecordSize();
	if (sz == -1)
		iec104_log(LOG_DEBUG, "Ошибка чтения размера данных dsp");
	else if (sz != iec104_dsp_data_size) {
		iec104_log(LOG_ERR, "Заданная длина данных dsp (%d) не совпадает с реальной (%d)", iec104_dsp_data_size, sz);
		uv_close((uv_handle_t*) clt, client_close);
		clean_exit_with_status(EXIT_FAILURE);
	} else {
		if (getActualData(0, sz, dsp_data))
			iec104_log(LOG_DEBUG, "Ошибка чтения текущих данных dsp");
		else 
			dsp_data_invalid = 0;
	}
}

float get_value_by_adr(int adr)
{
	float *analogs = (float *)((char *)dsp_data + iec104_analogs_offset);
	return analogs[adr-1];
}

// Общий/групповой опрос (соотв. группы 0/1)
void station_interrogation(client_t *clt, int group)
{
#ifdef DEBUG
	iec104_log(LOG_DEBUG, "station interrogation");
#endif

	if (group != 0 && group != 1) {
		iec104_log(LOG_WARNING, "Группа %d не содержит сигналов", group);
		return;
	}

	read_dsp_data(clt);

#if 0
	timeval cur_tv;	
	if (valid)
		get_dsp_time(&cur_tv);
	else
		gettimeofday(&cur_tv, NULL);
#endif

	char buf[APDU_MAX_LEN+2] = {0};
	apdu_t *out_apdu = (apdu_t *)buf;
	asdu_t *out_asdu = (asdu_t *)&buf[sizeof(apdu_t)];
	init_apdu(clt, out_apdu, AT_I);
	out_asdu->dui.type_id = M_ME_NC_1; 
	out_asdu->dui.code = COT_STATION_INTERROGATION + group;
	out_apdu->apci.len += sizeof(asdu_t);
#ifdef DEBUG
	iec104_log(LOG_DEBUG, "sizeof(apdu_t): %ld sizeof(asdu_t): %ld sizeof(inf_obj_t): %ld sizeof(type_13_t): %ld", sizeof(apdu_t), sizeof(asdu_t), sizeof(inf_obj_t), sizeof(type_13_t));
#endif
	for (int i = 0, j = 0; i < periodic_analogs_num; i++) {
		if (out_apdu->apci.len + sizeof(inf_obj_t) + 
			sizeof(type_13_t) > APDU_MAX_LEN) {
			j = 0;
			// Apdu заполнено, засунуть его в очередь 
			// и начать новое
			enqueue_apdu(clt, out_apdu);
			init_apdu(clt, out_apdu, AT_I);
			out_asdu->dui.type_id = M_ME_NC_1; 
			out_asdu->dui.code = COT_STATION_INTERROGATION;
			out_apdu->apci.len += sizeof(asdu_t);
		}
		// адрес объекта и значение
		inf_obj_t *obj = (inf_obj_t *)&buf[out_apdu->apci.len + 2];
		obj->adr = periodic_analogs[i];
		obj->inf_el[0].t13.sfpn = get_value_by_adr(periodic_analogs[i]); 
		obj->inf_el[0].t13.qual.QDS.IV = dsp_data_invalid;

		out_apdu->apci.len += sizeof(inf_obj_t) + 
			sizeof(type_13_t);
		out_asdu->dui.num = ++j;
	}
	enqueue_apdu(clt, out_apdu);
}

void read_single_data(client_t *clt, unsigned char obj_adr)
{
	read_dsp_data(clt);

	char buf[APDU_MAX_LEN+2] = {0};
	apdu_t *out_apdu = (apdu_t *)buf;
	asdu_t *out_asdu = (asdu_t *)&buf[sizeof(apdu_t)];
	init_apdu(clt, out_apdu, AT_I);
	out_asdu->dui.type_id = M_ME_NC_1; 
	out_asdu->dui.num = 1;
	out_asdu->dui.code = COT_REQUEST;
	inf_obj_t *obj = &out_asdu->inf_obj[0];
	obj->adr = obj_adr;
	obj->inf_el[0].t13.sfpn = get_value_by_adr(obj_adr); 
	obj->inf_el[0].t13.qual.QDS.IV = dsp_data_invalid;
	out_apdu->apci.len += sizeof(asdu_t) + 
		sizeof(inf_obj_t) + sizeof(type_13_t);
	enqueue_apdu(clt, out_apdu);
}

// Циклический опрос
void cyclic_poll(client_t *clt)
{
#ifdef DEBUG
	iec104_log(LOG_DEBUG, "cyclic poll");
#endif

	read_dsp_data(clt);

#if 0
	timeval cur_tv;	
	if (valid)
		get_dsp_time(&cur_tv);
	else
		gettimeofday(&cur_tv, NULL);
#endif

	char buf[APDU_MAX_LEN+2] = {0};
	apdu_t *out_apdu = (apdu_t *)buf;
	asdu_t *out_asdu = (asdu_t *)&buf[sizeof(apdu_t)];
	init_apdu(clt, out_apdu, AT_I);
	out_asdu->dui.type_id = M_ME_NC_1; 
	out_asdu->dui.code = COT_CYCLIC;
	out_asdu->dui.originator_adr = iec104_station_address;
	out_apdu->apci.len += sizeof(asdu_t);
	for (int i = 0, j = 0; i < periodic_analogs_num; i++) {
		if (out_apdu->apci.len + sizeof(inf_obj_t) + 
			sizeof(type_13_t) > APDU_MAX_LEN) {
			j = 0;
			// Apdu заполнено, засунуть его в очередь 
			// и начать новое
			enqueue_apdu(clt, out_apdu);
			init_apdu(clt, out_apdu, AT_I);
			out_asdu->dui.type_id = M_ME_NC_1; 
			out_asdu->dui.code = COT_CYCLIC;
			out_asdu->dui.originator_adr = iec104_station_address;
			out_apdu->apci.len += sizeof(asdu_t);
		}
		// адрес объекта и значение
		inf_obj_t *obj = (inf_obj_t *)&buf[out_apdu->apci.len + 2];
		obj->adr = periodic_analogs[i];
		obj->inf_el[0].t13.sfpn = get_value_by_adr(periodic_analogs[i]); 
		obj->inf_el[0].t13.qual.QDS.IV = dsp_data_invalid;

		out_apdu->apci.len += sizeof(inf_obj_t) + 
			sizeof(type_13_t);
		out_asdu->dui.num = ++j;
	}
	enqueue_apdu(clt, out_apdu);
}
