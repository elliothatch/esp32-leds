#include "pool.h"

#include "freertos/FreeRTOS.h"

/**
 * do I have to worry about alignment?
 * */

fp_pool_element* fp_pool_get_element(fp_pool* pool, fp_pool_id id) {
	return (fp_pool_element*)((char*)pool->elements + (sizeof(fp_pool_element) + pool->elementSize)*id);
}

fp_pool* fp_pool_init(unsigned int maxSize, unsigned int elementSize) {
	fp_pool* pool = malloc(sizeof(fp_pool));
	if(!pool) {
		printf("error: fp_pool_init: failed to allocate memory for pool\n");
		return NULL;
	}

	pool->elements = calloc(maxSize, sizeof(fp_pool_element) + elementSize);

	if(!pool->elements) {
		printf("error: fp_pool_init: failed to allocate memory for %ud elements (size %ud)\n", maxSize, elementSize);
		free(pool);
		return NULL;
	}

	pool->elementSize = elementSize;
	pool->maxSize = maxSize;
	pool->count = 1;
	pool->nextId = 1;

	fp_pool_get_element(pool, 0)->exists = true;

	return pool;
}

bool fp_pool_free(fp_pool* pool) {
	free(pool->elements);
	free(pool);
	return true;
}

void* fp_pool_get(fp_pool* pool, fp_pool_id id) {
	fp_pool_element* element = fp_pool_get_element(pool, id);
	if(id > pool->maxSize || !element->exists) {
		printf("error: fp_pool_get: invalid id: %d\n", id);
		return NULL;
	}

	return element+1; /* return memory right after the element */
}

fp_pool_id fp_pool_add(fp_pool* pool) {
	if(pool->count >= pool->maxSize) {
		printf("error: fp_pool_add: pool full. limit: %d\n", pool->maxSize);
		return 0;
	}

	fp_pool_id id = pool->nextId;
	if(id >= pool->maxSize) {
		id = 1;
	}

	fp_pool_element* element = fp_pool_get_element(pool, id);

	/* advance to next free element if necessary */
	while(element->exists) {
		printf("element %d already exists\n", id);
		id++;

		if(id >= pool->maxSize) {
			/** start the search back at the beginning.
			 * this shouldn't happen normally */
			/* assert */
			id = 1;
		}

		element = fp_pool_get_element(pool, id);
	}

	element->exists = true;
	pool->nextId = id+1;
	return id;
}

bool fp_pool_delete(fp_pool* pool, fp_pool_id id) {
	if(id == 0 || id > pool->maxSize) {
		return false;
	}

	fp_pool_element* element = fp_pool_get_element(pool, id);

	element->exists = false;
	if(id+1 < pool->nextId) {
		pool->nextId = id+1;
	}

	return true;
}

