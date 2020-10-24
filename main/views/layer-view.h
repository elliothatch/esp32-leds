#ifndef LAYER_VIEW_H
#define LAYER_VIEW_H

#include "../view.h"

/* fp: fresh pixel */

typedef enum {
	FP_BLEND_REPLACE,
	FP_BLEND_OVERWRITE, /* 0s overwrite other colors */
	FP_BLEND_ADD,
	FP_BLEND_MULTIPLY,
	FP_BLEND_ALPHA,
} fp_blend_mode;

typedef struct {
	fp_viewid view;
	fp_blend_mode blendMode;
	unsigned int offsetX;
	unsigned int offsetY;
	/** alpha for the layer. only used with FP_BLEND_ALPHA blendMode */
	uint8_t alpha;
} fp_layer;

typedef struct {
	unsigned int layerCount;
	fp_layer* layers;
	/** stores the result of render */
	fp_frameid frame;

} fp_layer_view_data;

fp_viewid fp_create_layer_view(
	unsigned int width,
	unsigned int height,
	unsigned int layerWidth,
	unsigned int layerHeight,
	unsigned int layerCount
);

fp_viewid fp_create_layer_view_composite(
	unsigned int width,
	unsigned int height,
	fp_viewid* layers,
	unsigned int layerCount
);

fp_frameid fp_layer_view_get_frame(fp_view* view);
bool fp_layer_view_render(fp_view* view);
bool fp_layer_view_onnext_render(fp_view* view);
bool fp_layer_view_free(fp_view* view);

static const fp_view_register_data fp_layer_view_register_data = {
	&fp_layer_view_get_frame,
	&fp_layer_view_render,
	&fp_layer_view_onnext_render,
	&fp_layer_view_free
};

#endif /* LAYER_VIEW_H */
