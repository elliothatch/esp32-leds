#include "frame-view.h"

#include "freertos/FreeRTOS.h"

fp_viewid fp_frame_view_create(unsigned int width, unsigned int height, rgb_color color) {
	fp_frame_view_data* frameData = malloc(sizeof(fp_frame_view_data));
	if(!frameData) {
		printf("error: fp_frame_view_create: failed to allocate memory for frameData\n");
		return 0;
	}

	frameData->frame = fp_create_frame(width, height, color);
	return fp_view_create(FP_VIEW_FRAME, false, frameData);
}

fp_viewid fp_frame_view_create_composite(fp_frameid frameid) {
	fp_frame_view_data* frameData = malloc(sizeof(fp_frame_view_data));
	if(!frameData) {
		printf("error: fp_frame_view_create_composite: failed to allocate memory for frameData\n");
		return 0;
	}

	frameData->frame = frameid;
	return fp_view_create(FP_VIEW_FRAME, true, frameData);
}

fp_frameid fp_frame_view_get_frame(fp_view* view) {
	return ((fp_frame_view_data*)view->data)->frame;
}

bool fp_frame_view_render(fp_view* view) {
	return true;
}

bool fp_frame_view_onnext_render(fp_view* view) {
	return true;
}

bool fp_frame_view_free(fp_view* view) {
	fp_frame_view_data* frameData = view->data;
	if(!view->composite) {
		fp_frame_free(frameData->frame);
	}

	free(frameData);

	return true;
}
