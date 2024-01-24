#include <stddef.h>
#include <inttypes.h>
#include <unistd.h>
//#include <errno.h>
#include <threads.h>

// debug only
#include <stdio.h>

//#include "sq.h"
#ifndef SQ_BASE_CAPACITY
#define SQ_BASE_CAPACITY (256) // make sure that you fit in a cache line to start!
#endif

#define qstorage_grow(T) qstorage_ ## T ## _grow
#define qstorage_decl(T) struct queue_storage_ ## T

#define define_queue(T) \
struct queue_storage_ ## T { 	\
	T* data;		\
	size_t max_capacity;	\
	size_t capacity;	\
	int64_t tip;		\
};				\
						\
struct simple_queue_ ## T {			\
	struct queue_storage_ ## T *details;	\
	mtx_t details_mutex;			\
};						\
	\
int qstorage_ ## T ## _init(struct queue_storage_ ## T **details, size_t max_capacity, size_t initial_capacity)	\
{								\
	struct queue_storage_ ## T *res = malloc(sizeof(*res));	\
	int status = 0;						\
	size_t elem_size = sizeof(T);				\
	if(!details || !res) {					\
		return -EINVAL;					\
	}							\
	if(initial_capacity == 0) {				\
		initial_capacity = SQ_BASE_CAPACITY;		\
	}							\
	if(!(res->data = calloc(initial_capacity, elem_size))){	\
		status = -ENOMEM;				\
		puts("cant calloc");				\
		printf("res->data=%p, inicap=%zu, elsz=%zu\n", (void*)res->data, initial_capacity, elem_size);				\
		goto cleanup_res;				\
	}							\
	res->max_capacity = max_capacity;			\
	res->capacity = initial_capacity;			\
	res->tip = -1;						\
	*details = res;						\
	goto done;						\
cleanup_res:							\
	free(res);						\
done:								\
	return status;						\
								\
}\
\
int qstorage_ ## T ## _destroy(struct queue_storage_ ## T **details)\
{\
	int status = 0;\
	if(!details) {\
		status = -EINVAL;\
		goto done;\
	}\
	qstorage_decl(T) *d = *details;\
	if(!d) {\
		status = -EINVAL;\
		goto null_out;\
	}\
	free(d->data);\
	free(d);\
null_out:\
	*details = NULL;\
done:\
	return status;\
\
}\
int qstorage_ ## T ## _grow(qstorage_decl(T) *details) \
{\
	int status = 0;\
	size_t max_capacity = details->max_capacity;\
	size_t new_capacity = details->capacity;\
	if(max_capacity == 0) {\
		max_capacity = SIZE_MAX;\
	}\
	if(new_capacity > SIZE_MAX >> 1) {\
		new_capacity = SIZE_MAX;\
		goto cap;\
	}\
	new_capacity >>= 1;\
	goto cap;\
cap:\
	if(new_capacity > max_capacity) {\
		new_capacity = max_capacity;\
	}\
	/*if we ever get a capacity > SIZE_MAX / sizeof(T), we're in trouble. unlikely to ever happen in production */\
	details->data = realloc(details->data, sizeof(T) * new_capacity);\
	if(!details->data) {\
		status = -ENOMEM;\
		goto done;\
	}\
	details->capacity = new_capacity;\
\
done:\
	return status;\
}\
\
int qstorage_ ## T ## _append(struct queue_storage_ ## T *details, T *elem)\
{\
	int status = 0;\
	details->tip++;\
	/* TODO: check if we need to increase the capacity.. */\
	if(details->tip >= details->capacity) {\
		status = qstorage_grow(T)(details);\
	}\
	/* TODO Use this memcpy approach or let user manage element lifetime? */\
	details->data[details->tip] = *elem;\
	return status;\
}\
int sq_ ## T ## _init(struct simple_queue_ ## T **q, size_t max_capacity, size_t initial_capacity)\
{\
	int status = 0;\
	struct simple_queue_ ## T *res = malloc(sizeof(*res));\
	if(!q || !res) {\
		return -EINVAL;\
	}\
	*q = res;\
\
	if((status = qstorage_ ## T ## _init(&res->details, max_capacity, initial_capacity))) {\
		goto cleanup_queue;\
	}\
	if((status = mtx_init(&res->details_mutex, mtx_timed))) {\
		goto cleanup_storage;\
	}\
	goto done;\
cleanup_storage:\
	qstorage_ ## T ## _destroy(&res->details);\
cleanup_queue:\
	free(res);\
done:\
	return status;\
\
\
}\
\
int sq_ ## T ## _destroy(struct simple_queue_ ## T **q)\
{\
	if(!q) return -EINVAL;\
	int status = 0;\
	status = qstorage_ ## T ## _destroy(&(*q)->details);\
	free(*q);\
	*q = NULL;\
	return status;\
}\
\
int sq_enqueue_ ## T (struct simple_queue_ ## T *q, T *elem)\
{\
	int status = 0;\
	/* lock the mutex */\
	/* add the data to the vector */\
	status = qstorage_ ## T ## _append(q->details, elem);\
	/* unlock the mutex */\
	return status;\
}\
int sq_peek_ ## T (sq_declare(T) *q, T **peek)\
{\
	if(!q || !q->details || !q->details->data || !peek){\
		*peek = NULL;\
		return -EINVAL;\
	}\
	if(q->details->tip < 0) {\
		*peek = NULL;\
		return 0;\
	}\
	*peek = &q->details->data[q->details->tip];\
	return 0;\
}\

struct element {
	int flags;
};

typedef struct element elem_s;

#define sq_declare(T) struct simple_queue_ ## T

#define sq_init(T) sq_ ## T ## _init

#define sq_enqueue(T) sq_enqueue_ ## T

#define sq_peek(T) sq_peek_ ## T

define_queue(elem_s)

int main(void)
{
	sq_declare(elem_s) *q;
	int res = sq_init(elem_s)(&q, 1000, 0);
	elem_s x = {.flags=1};
	res = sq_enqueue(elem_s)(q, &x);
	elem_s *peek = NULL;
	res = sq_peek(elem_s)(q, &peek);
	if(peek) {
		printf("peek->flags=%d\n", peek->flags);
	} else {
		puts("peek failed to find anything");
	}
	printf("res=%d\n", res);
	printf("simple calloc: %p\n", calloc(1, 1));
}
