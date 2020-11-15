#include <stdlib.h>

#define KB (1024)
#define MB (1024 * KB)
#define GB (1024 * MB)

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

struct mem_stack *mem_stack_init(void *storage, size_t size);
void mem_stack_frame_push(struct mem_stack *stack);
void mem_stack_frame_pop(struct mem_stack *stack);
void *mem_stack_push(struct mem_stack *stack, size_t size);

/* pool */

struct mem_pool_item {
	struct mem_pool_item *prev;
};

struct mem_pool {
	void *storage;
	size_t size;
	size_t item_size;
	struct mem_pool_item *first;
};

struct mem_pool *mem_pool_init(void *storage, size_t size, size_t item_size);
void *mem_pool_alloc(struct mem_pool *pool);
void *mem_pool_free(struct mem_pool *pool, void *p);

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

struct mem_freelist *mem_freelist_init(void *storage, size_t size);
void *mem_freelist_alloc(struct mem_freelist *freelist, size_t size);
void *mem_freelist_free(struct mem_freelist *freelist, void *p);
