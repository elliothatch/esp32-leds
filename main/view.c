#include "view.h"

#include "freertos/FreeRTOS.h"

#include "pool.h"

fp_view_register_data registered_views[FP_VIEW_TYPE_COUNT];

bool fp_view_register_type(fp_view_type viewType, fp_view_register_data registerData) {
	// TODO: disallow overwriting data
	if(viewType >= FP_VIEW_TYPE_COUNT) {
		return false;
	}

	registered_views[viewType] = registerData;
	return true;
}

/* register views.
 * TODO: allow more control over initialization?
 */

fp_pool* viewPool = NULL;
fp_view* zeroView;

bool fp_view_init(unsigned int capacity) {
	viewPool = fp_pool_init(capacity, sizeof(fp_view));
	zeroView = fp_pool_get(viewPool, 0);

	zeroView->type = FP_VIEW_FRAME;
	zeroView->id = 0;
	zeroView->parent = 0;
	zeroView->dirty = false;
	zeroView->composite = false;
	zeroView->data = NULL;

	return true;
}

fp_view* fp_view_get(fp_viewid id) {
	return fp_pool_get(viewPool, id);
}

fp_viewid fp_view_create(fp_view_type type, bool composite, fp_view_data* data) {
	fp_viewid id = fp_pool_add(viewPool);
	if(id == 0) {
		printf("error: fp_view_create: failed to add view\n");
		return 0;
	}

	fp_view* view = fp_pool_get(viewPool, id);

	view->type = type;
	view->id = id;
	view->parent = 0;
	view->dirty = true;
	view->composite = composite;
	view->data = data;

	if(DEBUG) {
		printf("view: create %d (%d/%d): type: %d\n", id, viewPool->count, viewPool->capacity, type);
	}

	return id;
}

bool fp_view_free(fp_viewid id) {
	fp_view* view = fp_view_get(id);
	bool result = registered_views[view->type].free_view(view);
	if(!result) {
		return result;
	}

	if(DEBUG) {
		printf("view: delete %d (%d/%d): type: %d\n", id, viewPool->count, viewPool->capacity, view->type);
	}

	return fp_pool_delete(viewPool, id);

}

/* can trigger re-render on dirty views */
fp_frameid fp_view_get_frame(fp_viewid id) {
	fp_view* view = fp_view_get(id);
	// TODO: handle invalid view id
	if(view->dirty) {
		fp_view_render(id);
	}

	if(view->type >= FP_VIEW_TYPE_COUNT) {
		return 0;
	}

	return registered_views[view->type].get_view_frame(view);
}

void fp_view_mark_dirty(fp_viewid id) {
	fp_view* view = fp_view_get(id);
	view->dirty = true;
	if(view->parent) {
		fp_view_mark_dirty(view->parent);
	}
}

bool fp_view_render(fp_viewid id) {
	fp_view* view = fp_pool_get(viewPool, id);
	if(!view) {
		return false;
	}

	if(view->type >= FP_VIEW_TYPE_COUNT) {
		return false;
	}

	return registered_views[view->type].render_view(view);

	view->dirty = false;

	return true;
}

bool fp_view_onnext_render(fp_viewid id) {
	fp_view* view = fp_pool_get(viewPool, id);
	if(!view) {
		return false;
	}

	if(view->type >= FP_VIEW_TYPE_COUNT) {
		return false;
	}

	return registered_views[view->type].onnext_render(view);

}
