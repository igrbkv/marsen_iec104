#ifndef ASDU_H_
#define ASDU_H_

typedef enum {
	ASDU_TYPE_NOT_DEFINED,
	M_ME_NC_1 = 13,		// measured, short FP number
	C_IC_NA_1 = 100,	// interrogation command
	C_RD_NA_1 = 102,	// read command
	C_CS_NA_1 = 103,	// clock synchronization command
	P_AC_NA_1 = 113,	// parameter activation
} ASDU_TYPE;

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
	unsigned short common_adr;	// не используется
								// (нет сложной адресации ASDU см.5-3)

} data_unit_id_t;

typedef struct _qualifier_t {
	union {
		struct {
			unsigned char QOI;
		};
		struct {
			unsigned char OV:1;	// Overflow
			unsigned char res:3;
			unsigned char BL:1;	// Blocked
			unsigned char SB:1;	// Substituted
			unsigned char NT:1;	// Not Topical
			unsigned char IV:1; // Invalid
		} QDS;
		struct {
			unsigned char QPA; // 3 - activation
		};
	};
} qualifier_t;

typedef struct __attribute__((__packed__)) _CP56Time2a_t {
	unsigned short ms;	// 0 ... 59999
	unsigned char min;	// 0 ... 59
	unsigned char hour;	// 0 ... 23
	unsigned char day;	// 1 ... 31
	unsigned char month;// 1 ... 12
	unsigned char year;	// 0 ... 99
} CP56Time2a_t;

typedef struct _type_100_t {
	qualifier_t qual;
} type_100_t;

typedef struct _type_102_t {
} type_102_t;

typedef struct _type_103_t {
	CP56Time2a_t time;
} type_103_t;

typedef struct _type_113_t {
	qualifier_t qual;
} type_113_t;

typedef struct __attribute__((__packed__)) _type_13_t {
	float sfpn;	//short FP number
	qualifier_t qual;
} type_13_t;

typedef struct _inf_el_t {
	union {
		type_13_t t13;
		type_100_t t100;
		type_102_t t102;
		type_103_t t103;
		type_113_t t113;
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

extern int check_asdu(client_t *clt, asdu_t *asdu, int size);
extern void process_asdu(client_t *clt, asdu_t *asdu, int size);
extern void station_interrogation(client_t *clt, int group);
extern void read_single_data(client_t *clt, unsigned char obj_adr);
extern void cyclic_poll(client_t *clt);
extern int adr_exist(unsigned short adr);
extern int activate_analog(unsigned short adr, int new_state);
extern void read_inf_obj(client_t *clt, unsigned short adr);
extern void sync_clock(CP56Time2a_t *t56);
extern unsigned char iec104_originator_adr;
#endif
