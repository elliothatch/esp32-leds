#include "pool.h"

/**
 * do I have to worry about alignment?
 * */

fp_pool_element* fp_pool_get_element(fp_pool* pool, fp_pool_id id) {
	return (fp_pool_element*)((char*)pool->elements + (sizeof(fp_pool_element) + pool->elementSize)*id);
}

fp_pool* fp_pool_init(unsigned int capacity, unsigned int elementSize, bool useSempahore) {
	fp_pool* pool = malloc(sizeof(fp_pool));
	if(!pool) {
		printf("error: fp_pool_init: failed to allocate memory for pool\n");
		return NULL;
	}

	pool->elements = calloc(capacity, sizeof(fp_pool_element) + elementSize);

	if(!pool->elements) {
		printf("error: fp_pool_init: failed to allocate memory for %ud elements (size %ud)\n", capacity, elementSize);
		free(pool);
		return NULL;
	}

	if(useSempahore) {
		pool->poolLock = xSemaphoreCreateBinary();
		if(pool->poolLock == NULL) {
			printf("error: fp_pool_init: failed to create semaphore\n");
		}
		else {
			xSemaphoreGive(pool->poolLock);
		}
	}
	else {
		pool->poolLock = NULL;
	}

	pool->elementSize = elementSize;
	pool->capacity = capacity;
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
	if(id > pool->capacity || !element->exists) {
		printf("error: fp_pool_get: invalid id: %d\n", id);
		return NULL;
	}

	return element+1; /* return memory right after the element */
}

fp_pool_id fp_pool_add(fp_pool* pool) {
	if(pool->count >= pool->capacity) {
		printf("error: fp_pool_add: pool full. limit: %d\n", pool->capacity);
		return 0;
	}

	if(pool->poolLock) {
		xSemaphoreTake(pool->poolLock, portMAX_DELAY);
	}

	fp_pool_id id = pool->nextId;
	if(id >= pool->capacity) {
		id = 1;
	}

	fp_pool_element* element = fp_pool_get_element(pool, id);

	/* advance to next free element if necessary */
	while(element->exists) {
		printf("element %d already exists\n", id);
		id++;

		if(id >= pool->capacity) {
			/** start the search back at the beginning.
			 * this shouldn't happen normally */
			/* assert */
			id = 1;
		}

		element = fp_pool_get_element(pool, id);
	}

	element->exists = true;
	pool->count++;
	pool->nextId = id+1;

	if(pool->poolLock) {
		xSemaphoreGive(pool->poolLock);
	}

	return id;
}

bool fp_pool_delete(fp_pool* pool, fp_pool_id id) {
	if(id == 0 || id > pool->capacity) {
		return false;
	}

	if(pool->poolLock) {
		xSemaphoreTake(pool->poolLock, portMAX_DELAY);
	}

	fp_pool_element* element = fp_pool_get_element(pool, id);

	element->exists = false;
	pool->count--;
	if(id < pool->nextId) {
		pool->nextId = id;
	}

	if(pool->poolLock) {
		xSemaphoreGive(pool->poolLock);
	}

	return true;
}

