#include "view.h"

#include "freertos/FreeRTOS.h"

/* fp_view viewPool[FP_VIEW_COUNT] = {{ FP_VIEW_FRAME, 0, false, {.FRAME = 0}}}; */
unsigned int viewCount = 1;
fp_view viewPool[FP_VIEW_COUNT] = {{ FP_VIEW_FRAME, 0, false, NULL}};

fp_view* fp_get_view(fp_viewid id) {
	if(id >= viewCount) {
		printf("error: fp_get_view: id %d too large, max id: %d\n", id, viewCount-1);
		return &viewPool[0];
	}

	return &viewPool[id];
}

fp_viewid fp_create_view(fp_view_type type, fp_viewid parent, fp_view_data data) {
	if(viewCount >= FP_VIEW_COUNT) {
		printf("error: fp_create_view: view pool full. limit: %d\n", FP_VIEW_COUNT);
		return 0;
	}

	fp_viewid id = viewCount++;

	viewPool[id].type = type;
	viewPool[id].parent = parent;
	viewPool[id].dirty = true;
	viewPool[id].data = data;

	if(DEBUG) {
		printf("create view %d: type: %d\n", id, type);
	}

	return id;
}

void fp_mark_view_dirty(fp_viewid id) {
	fp_view* view = fp_get_view(id);
	view->dirty = true;
	if(view->parent) {
		fp_mark_view_dirty(view->parent);
	}
}

fp_viewid fp_create_frame_view(unsigned int width, unsigned int height, rgb_color color) {
	fp_view_frame_data* frameData = malloc(sizeof(fp_view_frame_data));
	if(!frameData) {
		printf("error: fp_create_frame_view: failed to allocate memory for frameData\n");
		return 0;
	}

	frameData->frame = fp_create_frame(width, height, color);
	return fp_create_view(FP_VIEW_FRAME, 0, frameData);
}

fp_viewid fp_create_frame_view_composite(fp_frameid frameid) {
	fp_view_frame_data* frameData = malloc(sizeof(fp_view_frame_data));
	if(!frameData) {
		printf("error: fp_create_frame_view_composite: failed to allocate memory for frameData\n");
		return 0;
	}

	frameData->frame = frameid;
	return fp_create_view(FP_VIEW_FRAME, 0, frameData);
}

fp_viewid fp_create_screen_view(unsigned int width, unsigned int height) {
	fp_view_screen_data * screenData = malloc(sizeof(fp_view_screen_data));
	if(!screenData) {
		printf("error: fp_create_screen_view: failed to allocate memory for screenData\n");
		return 0;
	}

	screenData->frame = fp_create_frame(width, height, rgb(0,0,0));
	screenData->childView = 0;

	return fp_create_view(FP_VIEW_SCREEN, 0, screenData);
}
