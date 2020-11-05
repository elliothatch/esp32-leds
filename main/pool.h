#ifndef POOL_H
#define POOL_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/**
 * fp_pool
 * stores a pool of elements. new elements created are given an ID between 1 and capacity.
 * Id 0 is always populated, and should be set to the zero-element for your type after calling fp_pool_init
 *  */

typedef unsigned int fp_pool_id;

typedef struct {
	bool exists;
} fp_pool_element;

typedef struct {
	unsigned int capacity;
	unsigned int elementSize;
	SemaphoreHandle_t poolLock;

	void* elements;
	unsigned int count;
	fp_pool_id nextId; /* the smallest known available id. */
} fp_pool;

fp_pool* fp_pool_init(unsigned int capacity, unsigned int elementSize, bool useSempahore);

bool fp_pool_free(fp_pool* pool);

/** retrieve element. returns 0 if the element does not exist. element with id 0 always exists */
void* fp_pool_get(fp_pool* pool, fp_pool_id id);
/** creates a new element and assigns it an ID between 1 and capacity. Returns 0 if the element failed to create
 * always returns the lowest possible id--counts up from zero, but will recycle deleted IDs before assigning new IDs
 */
fp_pool_id fp_pool_add(fp_pool* pool);
bool fp_pool_delete(fp_pool* pool, fp_pool_id id);

#endif /* POOL_H */
