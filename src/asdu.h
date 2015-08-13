#ifndef ASDU_H_
#define ASDU_H_

typedef enum {
	ASDU_TYPE_NOT_DEFINED,
	C_IC_NA_1 = 100,	// interrogation command
	C_CI_NA_1 = 101,	// counter interrogation command
	C_CS_NA_1 = 103		// clock synchronization command
} ASDU_TYPE;

typedef struct _data_unit_id_t {
	unsigned char type_id;	// type identification
	struct {		// variable structure qualifier
		unsigned char num:7; // number of inf objects/elements
		unsigned char sq_bit:1; // 0/1 - objects/elements
	};
	struct {		// cause of transmission
		unsigned char code:6;
		unsigned char confirm_bit:1;
		unsigned char test_bit:1;
		unsigned char originator_adr;
	};
	unsigned short common_adr;

} data_unit_id_t;

typedef struct _qualifier_t {
	union {
		struct {
			unsigned char QOI;
		};
	};
} qualifier_t;

typedef struct _inf_el_t {
	union {
		qualifier_t q;
	};
} inf_el_t;

typedef struct __attribute__((__packed__)) _inf_obj_t {
	unsigned short adr;
	unsigned char not_used_adr_byte;
	inf_el_t inf_el[];
} inf_obj_t;

typedef struct _asdu_t {
	data_unit_id_t dui;
	inf_obj_t inf_obj[];
} asdu_t;

extern void process_asdu(client_t *clt, asdu_t *asdu, int size);

#endif
