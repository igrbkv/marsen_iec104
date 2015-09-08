#include <sys/queue.h>
#include <uv.h>
#include <time.h>

#include "iec104.h"
#include "client.h"
#include "apdu.h"
#include "asdu.h"
#include "log.h"

#include "debug.h"

// t2 timeout < t1 timeout < t3 timeout
int iec104_t1_timeout_s;
int iec104_t2_timeout_s;
int iec104_t3_timeout_s;
int iec104_tc_timeout_s;

static void t1_cb(uv_timer_t *t);
static void t1_u_cb(uv_timer_t *t);
static void t2_cb(uv_timer_t *t);
static void t3_cb(uv_timer_t *t);
static void tc_cb(uv_timer_t *t);

void start_t1(client_t *clt)
{
	uv_timer_start(&clt->t1, t1_cb, iec104_t1_timeout_s*1000, 0);
#ifdef DEBUG
	iec104_log(LOG_DEBUG, "start t1");
#endif
}

void start_t1_u(client_t *clt)
{
	uv_timer_start(&clt->t1_u, t1_u_cb, iec104_t1_timeout_s*1000, 0);
#ifdef DEBUG
	iec104_log(LOG_DEBUG, "start t1_u");
#endif
}

void start_tc(client_t *clt)
{
	uv_timer_start(&clt->tc, tc_cb, 
		iec104_tc_timeout_s*1000, 
		iec104_tc_timeout_s*1000);
#ifdef DEBUG
	iec104_log(LOG_DEBUG, "start tc");
#endif
}

// Рестарт t1 на остаток таймаута
// @param s - прошло сек с начала таймаута
void restart_t1(client_t *clt, int s)
{
	s = iec104_t1_timeout_s - s;
	if (s < 0)
		s = 0;
	uv_timer_start(&clt->t1, t1_cb, s*1000, 0);
#ifdef DEBUG
	iec104_log(LOG_DEBUG, "restart t1(%d)", s);
#endif
}
void start_t2(client_t *clt)
{
	uv_timer_start(&clt->t2, t2_cb, iec104_t2_timeout_s*1000, 0);
#ifdef DEBUG
	iec104_log(LOG_DEBUG, "start t2");
#endif
}

void start_t3(client_t *clt)
{
	uv_timer_start(&clt->t3, t3_cb, iec104_t3_timeout_s*1000, 0);
#ifdef DEBUG
	iec104_log(LOG_DEBUG, "start t3");
#endif
}

void t1_cb(uv_timer_t *t)
{
	client_t *clt = (client_t *)t->data;
	iec104_log(LOG_WARNING, "Timeout t1 expired");
	uv_close((uv_handle_t *)clt, client_close);
}

void t1_u_cb(uv_timer_t *t)
{
	client_t *clt = (client_t *)t->data;
	iec104_log(LOG_WARNING, "Timeout t1_u expired");
	uv_close((uv_handle_t *)clt, client_close);
}

void t2_cb(uv_timer_t *t)
{
	client_t *clt = (client_t *)t->data;
#ifdef DEBUG
	iec104_log(LOG_DEBUG, "Timeout t2 expired");
#endif
	init_apdu(clt, (apdu_t *)clt->ctl_apdu, AT_S);
	((apdu_t *)clt->ctl_apdu)->apci.s.nr = clt->nr;
	response(clt);
}

void t3_cb(uv_timer_t *t)
{
	client_t *clt = (client_t *)t->data;
	init_apdu(clt, (apdu_t *)clt->ctl_apdu, AT_U);
	((apdu_t *)clt->ctl_apdu)->apci.u.testfr_act = 1;
	start_t1_u(clt);
	response(clt);
}

void tc_cb(uv_timer_t *t)
{
	client_t *clt = (client_t *)t->data;
	cyclic_poll(clt);
	response(clt);
}

struct timespec CP56Time2a_to_timespec(CP56Time2a_t *t56)
{
	struct timespec ts;
	struct tm tm = {0};
	tm.tm_min = t56->min;
	tm.tm_hour = t56->hour;
	tm.tm_mday = t56->day;
	tm.tm_mon = t56->month - 1;
	tm.tm_year = t56->year + 100;
	tm.tm_sec = t56->ms/1000;
	ts.tv_sec = mktime(&tm);
	ts.tv_nsec = t56->ms%1000*1000000;
	return ts;
}

CP56Time2a_t timespec_to_CP56Time2a(struct timespec *ts)
{
	CP56Time2a_t t56;
	struct tm tm;
	localtime_r(&ts->tv_sec, &tm);
	t56.min = tm.tm_min;
	t56.hour = tm.tm_hour;
	t56.day = tm.tm_mday;
	t56.month = tm.tm_mon + 1;
	t56.year = tm.tm_year - 100;
	t56.ms = (ts->tv_sec%60)*1000 + ts->tv_nsec/1000000;
	return t56;
}

// Устанавливает новое время в системе 
// и возвращает в той же структуре старое 
void sync_clock(CP56Time2a_t *t56)
{
	struct timespec new_ts, old_ts;
	clock_gettime(CLOCK_REALTIME, &old_ts);
	new_ts = CP56Time2a_to_timespec(t56);
	clock_settime(CLOCK_REALTIME, &new_ts);
	*t56 = timespec_to_CP56Time2a(&old_ts);
}
