#include "frame-view.h"

#include "freertos/FreeRTOS.h"

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

fp_frameid fp_frame_view_get_frame(fp_view* view) {
	return ((fp_view_frame_data*)view->data)->frame;
}

bool fp_frame_view_render(fp_view* view) {
	return true;
}

bool fp_frame_view_onnext_render(fp_view* view) {
	return true;
}
