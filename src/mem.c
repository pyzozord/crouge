#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KB (1024)
#define MB (1024 * KB)
#define GB (1024 * MB)

void *main_memory;

/* stack */

struct StackItem {
	size_t size;
	struct StackItem *previous;
};

struct Stack { 
	void *storage;
	size_t size;
	struct StackItem *last;
};

struct Stack *stack_init(void *storage, size_t size) {
	size_t header_s = sizeof(struct Stack);
	struct Stack *header = storage;

	header->storage = (char *)storage + header_s;
	header->size = size - header_s;
	header->last = header->storage;
	header->last->previous = 0;
	header->last->size = 0;
	return header;
}

void *stack_push(struct Stack *stack, size_t size) {
	struct StackItem *next = (struct StackItem *)((char *)(stack->last+1) + stack->last->size);
	if ((char *)next > (char *)stack->storage + stack->size)
		return 0;
	next->size = size;
	next->previous = stack->last;
	stack->last = next;
	return next+1;
}

int stack_pop(struct Stack *stack) {
	if (!stack->last->previous)
		return 1;
	stack->last = stack->last->previous;
	return 1;
}

/* pool */

struct PoolItem {
	struct PoolItem *next;
};

struct Pool {
	void *storage;
	size_t size;
	size_t item_size;
	struct PoolItem *first;
};

size_t PoolItem_size(size_t item_size) {
	return sizeof(struct PoolItem) + item_size;
}

unsigned pool_max_items(size_t size, size_t item_size) {
	long result = (long)(size - sizeof(struct Pool)) / (long)PoolItem_size(item_size);
	return result < 0 ? 0 : result;
}

struct Pool *pool_init(void *storage, size_t size, size_t item_size) {
	unsigned count = pool_max_items(size, item_size);
	size_t PoolItem_s = PoolItem_size(item_size);

	if (size - sizeof(struct Pool) != count * PoolItem_s)
		return 0;
	size_t header_s = sizeof(struct Pool);
	struct Pool *header = storage;

	header->size = size;
	header->item_size = item_size;
	header->storage = (char *)storage + sizeof(struct Pool);
	header->first = header->storage;

	struct PoolItem *pi = header->first;

	for (int i = 1; i < count; pi = pi->next, i++)
		pi->next = (struct PoolItem *)((char *)pi + PoolItem_s);

	pi->next = 0;
	return header;
}

struct Pool *pool_initc(void *storage, size_t size, size_t item_size) {
	unsigned count = pool_max_items(size, item_size);
	if (count == 0) {
		return 0;
	}
	size_t PoolItem_s = PoolItem_size(item_size);
	size_t allocated = sizeof(struct Pool) + count * PoolItem_s;
	return pool_init(storage, allocated, item_size);
}

void *pool_alloc(struct Pool *pool) {
	struct PoolItem *free = pool->first;
	if (!free) 
		return 0;
	pool->first = pool->first->next;
	return free+1;
}

void *pool_free(struct Pool *pool, void *p) {
	struct PoolItem *item = (struct PoolItem *)((char *)p - sizeof(struct PoolItem));
	item->next = pool->first;
	pool->first = item;
}

/* TODO: free list */

int main() {
	main_memory = malloc(1 * GB);
	struct Stack *stack = stack_init(main_memory, 1 * GB); // entire memory is a stack

	void *pool_storage = stack_push(stack, 200 * MB);
	struct Pool *pool = pool_initc(pool_storage, 200 * MB, sizeof(int));

	int *i1 = pool_alloc(pool);
	int *i2 = pool_alloc(pool);
	int *i3 = pool_alloc(pool);

	*i1 = 11;
	*i2 = 22;
	*i3 = 33;

	printf("%d %d %d\n", *i1, *i2, *i3);

	pool_free(pool, i2);

	int *i4 = pool_alloc(pool);
	*i4 = 44;

	printf("%d %d %d\n", *i1, *i2, *i3); // i4 has been allocated in place of i2

	return 0;
}
