#include "dynamic-view.h"

#include "freertos/FreeRTOS.h"

fp_viewid fp_create_dynamic_view(
	unsigned int width,
	unsigned int height,
	bool (*renderFunc) (fp_view*),
	bool (*onnextRenderFunc) (fp_view*),
	void* data
) {
	fp_dynamic_view_data* dynamicData = malloc(sizeof(fp_dynamic_view_data));
	if(!dynamicData) {
		printf("error: fp_create_frame_view: failed to allocate memory for dynamicData\n");
		return 0;
	}

	dynamicData->frame = fp_create_frame(width, height, rgb(0, 0, 0));
	dynamicData->renderFunc = renderFunc;
	dynamicData->onnextRenderFunc = onnextRenderFunc;
	dynamicData->data = data;

	return fp_create_view(FP_VIEW_DYNAMIC, false, dynamicData);
}

fp_frameid fp_dynamic_view_get_frame(fp_view* view) {
	fp_dynamic_view_data* dynamicData = view->data;
	return dynamicData->frame;
}

bool fp_dynamic_view_render(fp_view* view) {
	fp_dynamic_view_data* dynamicData = view->data;
	return dynamicData->renderFunc(view);
}

bool fp_dynamic_view_onnext_render(fp_view* view) {
	fp_dynamic_view_data* dynamicData = view->data;
	return dynamicData->onnextRenderFunc(view);
}

bool fp_dynamic_view_free(fp_view* view) {
	fp_dynamic_view_data* dynamicData = view->data;

	fp_free_frame(dynamicData->frame);
	free(dynamicData);

	return true;
}
