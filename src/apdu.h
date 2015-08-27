#ifndef APDU_H_
#define APDU_H_

#define APDU_MAX_LEN 253
#define APDU_MAX_COUNT 32768

typedef enum {
	AT_I,
	AT_S,
	AT_U
} APDU_TYPE;

typedef struct _apci_t {
	char start_byte;
	unsigned char len;
	union {
		struct f_i {
			unsigned char  ft:1;
			unsigned short ns:15;
			unsigned char  res:1;
			unsigned short nr:15;
			} i;
		struct f_s {
			unsigned char  ft:1;
			unsigned short res1:15;
			unsigned char  res2:1;
			unsigned short nr:15;
		} s;
		struct f_u {
			unsigned char  ft1:1;
			unsigned char  ft2:1;
			unsigned char  startdt_act:1;
			unsigned char  startdt_con:1;
			unsigned char  stopdt_act:1;
			unsigned char  stopdt_con:1;
			unsigned char  testfr_act:1;
			unsigned char  testfr_con:1;
			unsigned char  res1;
			unsigned short res2;
		} u;
	};
} apci_t;

typedef struct _apdu_t {
	apci_t apci;
	char asdu[];
} apdu_t;

static inline APDU_TYPE apdu_type(apdu_t *apdu)
{
	if (apdu->apci.i.ft == 0)
		return AT_I;
	else if (apdu->apci.u.ft1 && apdu->apci.u.ft2)
		return AT_U;

	return AT_S;
}

extern void enqueue_apdu(client_t *clt, apdu_t *apdu);
extern int check_apdu(client_t *clt, const uv_buf_t *buf, ssize_t sz, int offset);
extern void init_apdu(client_t *clt, apdu_t *apdu, APDU_TYPE apdu_type);
extern void print_apdu(const char *head, const char *buf, int size);
#endif
