#ifndef WRITE_QUEUE_H_
#define WRITE_QUEUE_H_

typedef struct _write_queue_el_t { 
	STAILQ_ENTRY(_write_queue_el_t) next;
	void *data;
} write_queue_el_t;

STAILQ_HEAD(_write_queue_t, _write_queue_el_t);
typedef struct _write_queue_t write_queue_t;

extern void write_queue_init(write_queue_t *q);
extern void write_queue_destroy(write_queue_t *q);
extern void write_queue_append(write_queue_t *q, void *d);
#endif

