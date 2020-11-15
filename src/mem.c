#include <stdio.h>
#include <string.h>

#include "mem.h"

/* TODO: memory alignment for all allocators */

/* stack */

struct mem_stack *mem_stack_init(void *storage, size_t size) {
	struct mem_stack *header = storage;

	header->storage = (char *)storage + sizeof(struct mem_stack);
	header->size = size - sizeof(struct mem_stack);
	header->top = (char*)header->storage + sizeof(struct mem_stack_frame);
	header->last = header->storage;
	header->last->prev = 0;
	return header;
}

void mem_stack_frame_push(struct mem_stack *stack) {
	struct mem_stack_frame *frame = stack->top;
	stack->top = (char*)stack->top + sizeof(*frame);
	frame->prev = stack->last;
	stack->last = frame;
}

void mem_stack_frame_pop(struct mem_stack *stack) {
	stack->top = (char*)stack->last + sizeof(struct mem_stack_frame);
	stack->last = stack->last->prev;
}

void *mem_stack_push(struct mem_stack *stack, size_t size) {
	void *p = stack->top;
	stack->top = (char*)stack->top + size;
	return p;
}

/* pool */

struct mem_pool *mem_pool_init(void *storage, size_t size, size_t item_size) {
	if (size < sizeof(struct mem_pool) || size - sizeof(struct mem_pool) < item_size || item_size < sizeof(struct mem_pool_item))
		return 0;

	unsigned count = (size - sizeof(struct mem_pool)) / item_size;

	struct mem_pool *header = storage;

	header->size = size - sizeof(struct mem_pool);
	header->item_size = item_size;
	header->storage = (char *)storage + sizeof(struct mem_pool);
	header->first = header->storage;

	struct mem_pool_item *pi = header->first;
	pi->prev = 0;
	return header;
}

void *mem_pool_alloc(struct mem_pool *pool) {
	struct mem_pool_item *free = pool->first;
	if (free->prev)
		pool->first = pool->first->prev;
	else {
		struct mem_pool_item *first = (struct mem_pool_item*)((char*)free + pool->item_size);
		if ((char*)first > (char*)(pool+1) + pool->size) {
			return 0;
		}
		pool->first = first;
		pool->first->prev = 0;
	}
	return free;
}

void *mem_pool_free(struct mem_pool *pool, void *p) {
	struct mem_pool_item *prev = pool->first;
	pool->first = p;
	pool->first->prev = prev;
}

/* free list */

struct mem_freelist *mem_freelist_init(void *storage, size_t size) {
	size_t header_s = sizeof(struct mem_freelist);
	struct mem_freelist *header = storage;

	header->size = size - sizeof(struct mem_freelist);
	header->storage = (char *)storage + sizeof(struct mem_freelist);
	header->first = header->storage;
	header->first->next = 0;
	header->first->size = header->size - sizeof(struct mem_freelist_item);

	return header;
}

void *mem_freelist_alloc(struct mem_freelist *freelist, size_t size) {
	size_t total_size = size + sizeof(struct mem_freelist_item);
	struct mem_freelist_item *curr = freelist->first;

	for (; curr && curr->size < total_size; curr = curr->next)
		;
	if (!curr) 
		return 0;

	curr->size -= total_size;
	curr = (struct mem_freelist_item*)((char *)curr + curr->size + sizeof(struct mem_freelist_item));
	curr->size = size;
	return curr+1;
}

void *mem_freelist_free(struct mem_freelist *freelist, void *p) {
	struct mem_freelist_item *curr = freelist->first;
	struct mem_freelist_item *freed = (struct mem_freelist_item*)p-1;

	for (; curr->next && curr->next < freed; curr = curr->next)
		;

        if ((struct mem_freelist_item*)((char *)(freed+1) + freed->size) == curr->next) {
                freed->size += curr->next->size + sizeof(struct mem_freelist_item);
                freed->next = curr->next->next;
        } else
                freed->next = curr->next;

        if ((char*)(curr+1) + curr->size == (char *)freed) {
                curr->size += freed->size + sizeof(struct mem_freelist_item);
                curr->next = freed->next;
        } else  
                curr->next = freed;
}
