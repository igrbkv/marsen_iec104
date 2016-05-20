#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <json-c/json.h>

#include "iec104.h"
#include "log.h"

//#include "debug.h"

#define USE_ITERATOR

static json_object *parse_settings_json(const char *path);

// разбор текущего файла конфигурации
int settings_init()
{
	if (access(iec104_conffile, F_OK) == -1) {
		iec104_log(LOG_WARNING, "Фaйл %s не существует", iec104_conffile);
		return 0;
	}

	json_object *settings = parse_settings_json(iec104_conffile);
	if (settings == NULL) {
		iec104_log(LOG_WARNING, "Ошибка разбора файла конфигурации %s", iec104_conffile);
		return 0;
	}

	json_object *iec104_obj, *obj;

	if (json_object_object_get_ex(settings, "iec104", &iec104_obj) == FALSE) {
		iec104_log(LOG_WARNING, "В файле конфигурации oтсутствует секция параметров программы, используются значения по умолчанию");
		goto end;
	}

#ifdef USE_ITERATOR
    struct json_object_iterator it = json_object_iter_begin(iec104_obj);
	struct json_object_iterator it_end = json_object_iter_end(iec104_obj);
	while (!json_object_iter_equal(&it, &it_end)) {
		const char *key = json_object_iter_peek_name(&it);
		obj = json_object_iter_peek_value(&it);
		enum json_type type = json_object_get_type(obj);
		int unknown = 0;

		if (type == json_type_int) {
			int ival = json_object_get_int(obj);
			if (strcmp(key, "debug") == 0)
				iec104_debug = ival;
			else if (strcmp(key, "k") == 0)
				iec104_k = ival;
			else if (strcmp(key, "w") == 0)
				iec104_w = ival;
			else if (strcmp(key, "analogs-offset") == 0)
				iec104_analogs_offset = ival;
			else if (strcmp(key, "dsp-data-size") == 0)
				iec104_dsp_data_size = ival;
			else if (strcmp(key, "t1") == 0)
				iec104_t1_timeout_s = ival;
			else if (strcmp(key, "t2") == 0)
				iec104_t2_timeout_s = ival;
			else if (strcmp(key, "t3") == 0)
				iec104_t3_timeout_s = ival;
			else if (strcmp(key, "station-address") == 0)
				iec104_station_address = ival;
			else if (strcmp(key, "cyclic-poll-period") == 0)
				iec104_tc_timeout_s = ival;
			else 
				unknown = 1;
		} else if (type == json_type_string) {
			const char *sval = json_object_get_string(obj);
			if (strcmp(key, "periodic-analogs") == 0) {
				free(iec104_periodic_analogs);
				iec104_periodic_analogs = strdup(sval);
			} else
				unknown = 1;
		} else
			unknown = 1;

		if (unknown)
            iec104_log(LOG_WARNING, "Неизвестный параметр: %s%s", key, json_object_to_json_string(obj));

		json_object_iter_next(&it);
	}
#else
	if (json_object_object_get_ex(iec104_obj, "debug", &obj) == TRUE) 
		iec104_debug = json_object_get_int(obj);

	if (json_object_object_get_ex(iec104_obj, "k", &obj) == TRUE) 
		iec104_k = json_object_get_int(obj);

	if (json_object_object_get_ex(iec104_obj, "w", &obj) == TRUE) 
		iec104_w = json_object_get_int(obj);

	if (json_object_object_get_ex(iec104_obj, "analogs-offset", &obj) == TRUE) 
		iec104_analogs_offset = json_object_get_int(obj);
	if (json_object_object_get_ex(iec104_obj, "dsp-data-size", &obj) == TRUE) 
		iec104_dsp_data_size = json_object_get_int(obj);

	if (json_object_object_get_ex(iec104_obj, "periodic-analogs", &obj) == TRUE) {
		free(iec104_periodic_analogs);
		iec104_periodic_analogs = strdup(json_object_get_string(obj));
	}

	if (json_object_object_get_ex(iec104_obj, "t1", &obj) == TRUE)
		iec104_t1_timeout_s = json_object_get_int(obj);

	if (json_object_object_get_ex(iec104_obj, "t2", &obj) == TRUE)
		iec104_t2_timeout_s = json_object_get_int(obj);

	if (json_object_object_get_ex(iec104_obj, "t3", &obj) == TRUE)
		iec104_t3_timeout_s = json_object_get_int(obj);

	
	if (json_object_object_get_ex(iec104_obj, "station-address", &obj) == TRUE)
		iec104_station_address = json_object_get_int(obj);

	if (json_object_object_get_ex(iec104_obj, "cyclic-poll-period", &obj) == TRUE)
		iec104_tc_timeout_s = json_object_get_int(obj);
#endif	

end:
	json_object_put(settings);

	return 0;
}

void settings_close()
{
	free(iec104_periodic_analogs);
	iec104_periodic_analogs = NULL;
}

json_object *parse_settings_json(const char *path)
{
	FILE *fp = NULL;
	json_object *new_obj = NULL;
	char *line = NULL;
	json_tokener *tok = NULL;

#ifdef DEBUG
	iec104_log(LOG_DEBUG, "parse_settings(%s)", path);
#endif
	fp = fopen(path, "r");
	if (!fp) {
		iec104_log(LOG_WARNING, "fopen(%s) failed!: %s", path, strerror(errno));
		goto err;
	}

	size_t len = 0;
	ssize_t read;

	tok = json_tokener_new();
	enum json_tokener_error jerr;
	while ((read = getline(&line, &len, fp)) != -1) {
		new_obj = json_tokener_parse_ex(tok, line, read);
		if ((jerr = json_tokener_get_error(tok)) != json_tokener_continue)
			break;
	}
	if (jerr != json_tokener_success) {
		iec104_log(LOG_DEBUG, "json_tokener_parse_ex() failed: %s", json_tokener_error_desc(jerr));
		goto err;
	}

err:
	if (line)
		free(line);
	if (fp)
		fclose(fp);
	if (tok)
		json_tokener_free(tok);

	return new_obj;
}
