#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#define _GNU_SOURCE
#define _DEFAULT_SOURCE		// for daemon
#include <unistd.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/queue.h>
#include <uv.h>

#include "iec104.h"
#include "tcp_server.h"
#include "cur_values.h"
#include "log.h"
#include "settings.h"

#define IEC104_CONFFILE IEC104_CONFPATH "/settings.json"
#define IEC104_PIDFILE "/var/run/iec104.pid" 

#define DEFAULT_ANALOGS_OFFSET 180
#define DEFAULT_DSP_DATA_SIZE 50592
#define DEFAULT_PERIODIC_ANALOGS "1,62-135,12584-12603"
#define DEFAULT_K 12
#define DEFAULT_W 8
#define DEFAULT_STATION_ADDRESS 0
#define DEFAULT_T1 15
#define DEFAULT_T2 10
#define DEFAULT_T3 20
#define DEFAULT_CYCLIC_POLL_PERIOD 3

static void default_init();
static int handle_cmdline(int *argc, char ***argv);
static void open_log(void);
static void clean_exit(int sig);
static void reload_conf(int sig);
static int create_pidfile();

const char *progname;
const char *iec104_confpath = IEC104_CONFPATH;
const char *iec104_conffile = IEC104_CONFFILE;
static const char *pidfile = IEC104_PIDFILE; 

uv_loop_t *loop = NULL;

static int foreground;
static int restart;
 

int main(int argc, char **argv)
{
	int exit_status = EXIT_FAILURE;
	/* learn who we really are */
	progname = (const char *)strrchr(argv[0], '/');
	progname = progname ? (progname + 1) : argv[0];

	default_init();
	handle_cmdline(&argc, &argv);
	open_log();


	if (!foreground) {
		if (daemon(0, 0) == -1) {
			iec104_log(LOG_ERR, "daemon(): %s", strerror(errno));
			goto end;
		}
	}
	
	/* trap key signals */
	signal(SIGHUP, reload_conf);
	signal(SIGINT, clean_exit);
	signal(SIGQUIT, clean_exit);
	signal(SIGTERM, clean_exit);
	signal(SIGPIPE, SIG_IGN);

	/* create our pidfile */
	if (!foreground && create_pidfile() < 0)
		goto end;

	iec104_log(LOG_INFO, "started");		

	loop = uv_default_loop();

	/* read configuration */
	if (settings_init() == -1)
		goto end;

	if (cur_values_init() == -1 || 
		tcp_server_init() == -1)
		goto  end;

	uv_run(loop, UV_RUN_DEFAULT);

	exit_status = EXIT_SUCCESS;

end:	
	clean_exit_with_status(exit_status);

	return 0;
}

static void default_init()
{
    foreground = 0;
	iec104_debug = 0;

	iec104_analogs_offset = DEFAULT_ANALOGS_OFFSET;
	iec104_dsp_data_size = DEFAULT_DSP_DATA_SIZE;
	iec104_periodic_analogs = strdup(DEFAULT_PERIODIC_ANALOGS);
	iec104_k = DEFAULT_K;
	iec104_w = DEFAULT_W;
	iec104_station_address = DEFAULT_STATION_ADDRESS;
	iec104_t1_timeout_s = DEFAULT_T1;
	iec104_t2_timeout_s = DEFAULT_T2;
	iec104_t3_timeout_s = DEFAULT_T3;
	iec104_tc_timeout_s = DEFAULT_CYCLIC_POLL_PERIOD;
}

static void open_log(void)
{
	int log_opts;

	/* open the syslog */
	log_opts = LOG_CONS|LOG_NDELAY;
	if (iec104_debug) {
		log_opts |= LOG_PERROR;
	}
	openlog(PACKAGE, log_opts, LOG_DAEMON);
}

static void cleanup()
{

	tcp_server_close();
	cur_values_close();
	settings_close();

	if (loop != NULL) {
		uv_loop_close(loop);
		loop = NULL;
	}
}

void clean_exit_with_status(int status)
{
	cleanup();

	iec104_log(LOG_NOTICE, "exiting");
	unlink(pidfile);
	exit(status);
}

static void clean_exit(int sig)
{
	iec104_log(LOG_NOTICE, "clean exit");
	clean_exit_with_status(EXIT_SUCCESS);
}

static void reload_conf(int sig __attribute__((unused)))
{
	restart = 1;
	iec104_log(LOG_NOTICE, "reloading configuration");
	cleanup();

	default_init();

	loop = uv_default_loop();

	if (settings_init() == -1 ||
		cur_values_init() == -1 ||
		tcp_server_init() == -1)
		goto err;
	uv_run(loop, UV_RUN_DEFAULT);

	return;
err:
	clean_exit_with_status(EXIT_FAILURE);
}

/*
 * Parse command line arguments
 */
static int handle_cmdline(int *argc, char ***argv)
{
	struct option opts[] = {
		{"debug", 0, 0, 'd'},
		{"debug-foreground", 0, 0, 'D'},
		{"foreground", 0, 0, 'f'},
		{"version", 0, 0, 'v'},
		{"help", 0, 0, 'h'},
		{NULL, 0, 0, 0},
	};
	const char *opts_help[] = {
		"Increase debugging level.",/* debug */
		"Increase debugging level (implies -f).",/* debug-foreground */
		"Run in the foreground.",		/* foreground */
		"Print version information.",		/* version */
		"Print this message.",			/* help */
	};
	struct option *opt;
	const char **hlp;
	//char *ptr;
	int max, size;

	for (;;) {
		int i;
		i = getopt_long(*argc, *argv,
		    "dDfvh", opts, NULL);
		if (i == -1) {
			break;
		}
		switch (i) {
		case 'd':
			iec104_debug++;
			break;
		case 'D':
			foreground = 1;
			iec104_debug++;
            log_to_stderr = 1;
			break;
		case 'f':
			foreground = 1;
			break;
		case 'v':
			printf(PACKAGE "-" VERSION "\n");
			exit(EXIT_SUCCESS);
		case 'h':
		default:
			fprintf(stderr, "Usage: %s [OPTIONS]\n", progname);
			max = 0;
			for (opt = opts; opt->name; opt++) {
				size = strlen(opt->name);
				if (size > max)
					max = size;
			}
			for (opt = opts, hlp = opts_help;
			     opt->name;
			     opt++, hlp++) {
				fprintf(stderr, "  -%c, --%s",
					opt->val, opt->name);
				size = strlen(opt->name);
				for (; size < max; size++)
					fprintf(stderr, " ");
				fprintf(stderr, "  %s\n", *hlp);
			}
			exit(EXIT_FAILURE);
			break;
		}
	}

	*argc -= optind;
	*argv += optind;

	return 0;
}

int create_pidfile()
{
	int fd;

	/* JIC */
	unlink(pidfile);

	/* open the pidfile */
	fd = open(pidfile, O_WRONLY|O_CREAT|O_EXCL, 0644);
	if (fd >= 0) {
		FILE *f;

		/* write our pid to it */
		f = fdopen(fd, "w");
		if (f != NULL) {
			fprintf(f, "%d\n", getpid());
			fclose(f);
			/* leave the fd open */
			return 0;
		}
		close(fd);
	}

	/* something went wrong */
	iec104_log(LOG_ERR, "can't create pidfile %s: %s",
		    pidfile, strerror(errno));
	return -1;
}
