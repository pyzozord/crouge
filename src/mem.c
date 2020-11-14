#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KB (1024)
#define MB (1024 * KB)
#define GB (1024 * MB)

void *main_memory;

/* stack */

struct mem_stack_frame {
	struct mem_stack_frame *prev;
};

struct mem_stack { 
	void *storage;
	size_t size;
	struct mem_stack_frame *last;
	void *top;
};

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

struct mem_pool_item {
	struct mem_pool_item *next;
};

struct mem_pool {
	void *storage;
	size_t size;
	size_t item_size;
	struct mem_pool_item *first;
};

size_t mem_pool_item_size(size_t item_size) {
	return sizeof(struct mem_pool_item) + item_size;
}

unsigned mem_pool_max_items(size_t size, size_t item_size) {
	long result = (long)(size - sizeof(struct mem_pool)) / (long)mem_pool_item_size(item_size);
	return result < 0 ? 0 : result;
}

struct mem_pool *mem_pool_init(void *storage, size_t size, size_t item_size) {
	unsigned count = mem_pool_max_items(size, item_size);
	size_t mem_pool_item_s = mem_pool_item_size(item_size);

	if (size - sizeof(struct mem_pool) != count * mem_pool_item_s)
		return 0;
	size_t header_s = sizeof(struct mem_pool);
	struct mem_pool *header = storage;

	header->size = size;
	header->item_size = item_size;
	header->storage = (char *)storage + sizeof(struct mem_pool);
	header->first = header->storage;

	struct mem_pool_item *pi = header->first;

	for (int i = 1; i < count; pi = pi->next, i++)
		pi->next = (struct mem_pool_item *)((char *)pi + mem_pool_item_s);

	pi->next = 0;
	return header;
}

struct mem_pool *mem_pool_initc(void *storage, size_t size, size_t item_size) {
	unsigned count = mem_pool_max_items(size, item_size);
	if (count == 0) {
		return 0;
	}
	size_t mem_pool_item_s = mem_pool_item_size(item_size);
	size_t allocated = sizeof(struct mem_pool) + count * mem_pool_item_s;
	return mem_pool_init(storage, allocated, item_size);
}

void *mem_pool_alloc(struct mem_pool *pool) {
	struct mem_pool_item *free = pool->first;
	if (!free) 
		return 0;
	pool->first = pool->first->next;
	return free+1;
}

void *mem_pool_free(struct mem_pool *pool, void *p) {
	struct mem_pool_item *item = (struct mem_pool_item *)((char *)p - sizeof(struct mem_pool_item));
	item->next = pool->first;
	pool->first = item;
}

/* free list */

struct mem_freelist_item {
	struct mem_freelist_item *next;
	size_t size;
};

struct mem_freelist {
	void *storage;
	size_t size;
	struct mem_freelist_item *first;
};

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

int main() {
	void *main_memory = malloc(1 * GB);
	struct mem_stack *stack = mem_stack_init(main_memory, 1 * GB);
	printf("stack %p\n", (void*) stack);

	printf("stack->top %p\n", (void*) stack->top);
	printf("stack->last %p\n", (void*) stack->last);

	int *i1 = mem_stack_push(stack, sizeof(int));
	*i1 = 11;
	int *i2 = mem_stack_push(stack, sizeof(int));
	*i2 = 22;

	printf("%d %d\n", *i1, *i2);

	mem_stack_frame_push(stack);

	int *i3 = mem_stack_push(stack, sizeof(int));
	*i3 = 33;
	int *i4 = mem_stack_push(stack, sizeof(int));
	*i4 = 44;

	printf("%d %d %d %d\n", *i1, *i2, *i3, *i4);

	printf("stack->top %p\n", (void*) stack->top);
	printf("stack->last %p\n", (void*) stack->last);

	mem_stack_frame_pop(stack);

	int *i5 = mem_stack_push(stack, sizeof(int));
	*i5 = 55;
	int *i6 = mem_stack_push(stack, sizeof(int));
	*i6 = 66;

	printf("%d %d %d %d %d %d\n", *i1, *i2, *i3, *i4, *i5, *i6);

	printf("stack->top %p\n", (void*) stack->top);
	printf("stack->last %p\n", (void*) stack->last);
}
