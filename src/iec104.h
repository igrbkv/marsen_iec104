#ifndef IEC104_H_
#define IEC104_H_

#define PACKAGE 		"iec104"

#include <uv.h>

extern int iec104_debug;
extern const char *progname;
extern uv_loop_t *loop;

#define IEC104_PORT 2404
#define IEC104_CONFPATH "/etc/opt/marsenergo"
extern const char *iec104_confpath;
extern const char *iec104_conffile;

extern int iec104_k;
extern int iec104_w;
extern int iec104_analogs_offset;
extern int iec104_dsp_data_size;
extern char *iec104_periodic_analogs;
extern int iec104_t1_timeout_s;
extern int iec104_t2_timeout_s;
extern int iec104_t3_timeout_s;
extern int iec104_tc_timeout_s;
extern unsigned short iec104_station_address;

extern void clean_exit_with_status(int status);

#endif /* IEC104_H_ */
