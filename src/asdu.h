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
	struct f_vsq {		// variable structure qualifier
		unsigned char num:7; // number of inf objects/elements
		unsigned char sq_bit:1; // 0/1 - objects/elements
	} vsq;
	struct f_cot {		// cause of transmission
		unsigned char code:6;
		unsigned char confirm_bit:1;
		unsigned char test_bit:1;
		unsigned char originator_adr;
	} cot;
	unsigned short common_adr;

} data_unit_id_t;

typedef struct _inf_obj_t {
	unsigned short adr;
	unsigned char not_used_adr_byte;
	char data[];
} inf_obj_t;

typedef struct _qualifier {
	union {
		struct {
			unsigned char QOI;
		};
	};
} qualifier;

typedef struct _inf_el_t {
	union {
		qualifier q;
	};
} inf_el_t;

typedef struct _asdu_t {
	data_unit_id_t dui;
	inf_obj_t data[];
} asdu_t;

#endif
