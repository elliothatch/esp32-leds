#include "view.h"

#include "freertos/FreeRTOS.h"

#include "pool.h"

fp_view_register_data registered_views[FP_VIEW_TYPE_COUNT];

bool fp_register_view_type(fp_view_type viewType, fp_view_register_data registerData) {
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

fp_view* fp_get_view(fp_viewid id) {
	return fp_pool_get(viewPool, id);
}

fp_viewid fp_create_view(fp_view_type type, bool composite, fp_view_data* data) {
	fp_viewid id = fp_pool_add(viewPool);
	if(id == 0) {
		printf("error: fp_create_view: failed to add view\n");
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
		printf("create view %d: type: %d\n", id, type);
	}

	return id;
}

bool fp_free_view(fp_viewid id) {
	fp_view* view = fp_get_view(id);
	bool result = registered_views[view->type].free_view(view);
	if(!result) {
		return result;
	}

	return fp_pool_delete(viewPool, id);
}

/* can trigger re-render on dirty views */
fp_frameid fp_get_view_frame(fp_viewid id) {
	fp_view* view = fp_get_view(id);
	// TODO: handle invalid view id
	if(view->dirty) {
		fp_render_view(id);
	}

	if(view->type >= FP_VIEW_TYPE_COUNT) {
		return 0;
	}

	return registered_views[view->type].get_view_frame(view);
}

void fp_mark_view_dirty(fp_viewid id) {
	fp_view* view = fp_get_view(id);
	view->dirty = true;
	if(view->parent) {
		fp_mark_view_dirty(view->parent);
	}
}

bool fp_render_view(fp_viewid id) {
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

bool fp_onnext_render(fp_viewid id) {
	fp_view* view = fp_pool_get(viewPool, id);
	if(!view) {
		return false;
	}

	if(view->type >= FP_VIEW_TYPE_COUNT) {
		return false;
	}

	return registered_views[view->type].onnext_render(view);

}
