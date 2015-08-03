#include <stdlib.h>
#include <sys/queue.h>

#include "write_queue.h"

void write_queue_init(write_queue_t *q)
{
	STAILQ_INIT(q);
}

void write_queue_destroy(write_queue_t *q)
{
	while (!STAILQ_EMPTY(q)) {
		write_queue_el_t *fe = STAILQ_FIRST(q);
		STAILQ_REMOVE_HEAD(q, next);
		free(fe->data);
		free(fe);
	}
}

void write_queue_append(write_queue_t *q, void *d)
{
	write_queue_el_t *new = (write_queue_el_t *)malloc(sizeof(write_queue_el_t));
	new->data = d;
	STAILQ_INSERT_TAIL(q, new, next);
}
