#include <stddef.h>
#include <inttypes.h>
#include <unistd.h>
//#include <errno.h>
#include <threads.h>

// debug only
#include <stdio.h>

//#include "sq.h"
struct queue_storage_ {
	void* data;
	size_t elem_size;
	size_t max_capacity;
	size_t capacity;
	int64_t tip;
};

struct simple_queue {
	struct queue_storage_ *details;
	mtx_t details_mutex;
};

struct element {
	int flags;
};

#ifndef SQ_BASE_CAPACITY
#define SQ_BASE_CAPACITY (256) // make sure that you fit in a cache line to start!
#endif

int qstorage_init(struct queue_storage_ **details, size_t elem_size, size_t max_capacity, size_t initial_capacity)
{
	struct queue_storage_ *res = malloc(sizeof(*res));
	int status = 0;
	if(!details || !res) {
		return -EINVAL;
	}
	if(initial_capacity == 0) {
		initial_capacity = SQ_BASE_CAPACITY;
	}
	if(!(res->data = calloc(initial_capacity, elem_size))){
		status = -ENOMEM;
		puts("cant calloc");
		printf("res->data=%p, inicap=%zu, elsz=%zu\n", res->data, initial_capacity, elem_size);
		goto cleanup_res;
	}
	res->elem_size = elem_size;
	res->max_capacity = max_capacity;
	res->capacity = initial_capacity;
	res->tip = -1;
	goto done;
cleanup_res:
	free(res);
done:
	return status;

}

int qstorage_destroy(struct queue_storage_ **details)
{
	if(!details) {
		return -EINVAL;
	}
	struct queue_storage_ *d = *details;
	// TODO this assumes the elements dont need to be freed individually, which is wrong!
	for(int64_t i = d->tip; i >= 0; i--) {
		free(&d->data[i]);
	}
	free(d->data);
	free(d);
	*details = NULL;
	return 0;

}

int qstorage_append(struct queue_storage_ *details, void *elem)
{
	int status = 0;
	details->tip++;
	// TODO: check if we need to increase the capacity...
	details->data[details->tip * details->elem_size] = elem;
	return status;
}

int sq_init(struct simple_queue **q, size_t elem_size, size_t max_capacity, size_t initial_capacity)
{
	int status = 0;
	struct simple_queue *res = malloc(sizeof(*res));
	if(!q || !res) {
		return -EINVAL;
	}
	*q = res;

	if((status = qstorage_init(&res->details, elem_size, max_capacity, initial_capacity))) {
		goto cleanup_queue;
	}
	if((status = mtx_init(&res->details_mutex, mtx_timed))) {
		goto cleanup_storage;
	}
	goto done;
cleanup_storage:
	qstorage_destroy(&res->details);
cleanup_queue:
	free(res);
done:
	return status;


}

int sq_destroy(struct simple_queue **q)
{
	if(!q) return -EINVAL;
	int status = 0;
	//TODO free all of the elements
	status = qstorage_destroy(&(*q)->details);
	free((*q)->details);
	free(*q);
	*q = NULL;
	return status;
}

int sq_enqueue(struct simple_queue *q, void *elem)
{
	int status = 0;
	// lock the mutex
	// add the data to the vector
	status = qstorage_append(q->details, elem);
	// unlock the mutex
	return status;
}

int main(void)
{
	struct simple_queue *q;
	int res = sq_init(&q, sizeof(struct element), 1000, 0);
	printf("res=%d\n", res);
	printf("simple calloc: %p\n", calloc(1, 1));
}
