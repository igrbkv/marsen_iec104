#ifndef IEC104_H_
#define IEC104_H_

#define PACKAGE 		"iec104"

extern int iec104_debug;
extern const char *progname;
extern uv_loop_t *loop;

#define IEC104_PORT 2404
#define IEC104_CONFPATH "/etc/opt/marsenergo"
extern const char *iec104_confpath;

extern void clean_exit_with_status(int status);

#endif /* IEC104_H_ */
