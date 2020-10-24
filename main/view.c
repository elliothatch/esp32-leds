#include "view.h"

#include "freertos/FreeRTOS.h"

fp_view_register_data registered_views[FP_VIEW_TYPE_COUNT];

bool fp_register_view_type(fp_view_type viewType, fp_view_register_data registerData) {
	// TODO: disallow overwriting data
	if(viewType >= FP_VIEW_COUNT) {
		return false;
	}

	registered_views[viewType] = registerData;
	return true;
}

/* register views.
 * TODO: allow more control over initialization?
 */

/* fp_view viewPool[FP_VIEW_COUNT] = {{ FP_VIEW_FRAME, 0, false, {.FRAME = 0}}}; */
unsigned int viewCount = 1;
fp_view viewPool[FP_VIEW_COUNT] = {{ FP_VIEW_FRAME, 0, 0, false, false, NULL}};

fp_view* fp_get_view(fp_viewid id) {
	if(id >= viewCount) {
		printf("error: fp_get_view: id %d too large, max id: %d\n", id, viewCount-1);
		return &viewPool[0];
	}

	return &viewPool[id];
}

fp_viewid fp_create_view(fp_view_type type, bool composite, fp_view_data* data) {
	if(viewCount >= FP_VIEW_COUNT) {
		printf("error: fp_create_view: view pool full. limit: %d\n", FP_VIEW_COUNT);
		return 0;
	}

	fp_viewid id = viewCount++;

	viewPool[id].type = type;
	viewPool[id].id = id;
	viewPool[id].parent = 0;
	viewPool[id].dirty = true;
	viewPool[id].composite = composite;
	viewPool[id].data = data;

	if(DEBUG) {
		printf("create view %d: type: %d\n", id, type);
	}

	return id;
}

bool fp_free_view(fp_viewid id) {
	fp_view* view = fp_get_view(id);
	bool result = registered_views[view->type].free_view(view);
	// TODO: clean up view from pool
	return result && true;
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
	if(id >= viewCount) {
		return false;
	}

	fp_view* view = &viewPool[id];

	if(view->type >= FP_VIEW_TYPE_COUNT) {
		return false;
	}

	return registered_views[view->type].render_view(view);

	view->dirty = false;

	return true;
}

bool fp_onnext_render(fp_viewid id) {
	if(id >= viewCount) {
		return false;
	}

	fp_view* view = &viewPool[id];

	if(view->type >= FP_VIEW_TYPE_COUNT) {
		return false;
	}

	return registered_views[view->type].onnext_render(view);

}
