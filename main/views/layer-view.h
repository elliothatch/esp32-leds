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

} fp_view_layer_data;

fp_viewid fp_create_layer_view(fp_viewid* views, unsigned int layerCount, unsigned int width, unsigned int height, unsigned int layerWidth, unsigned int layerHeight);

#endif /* LAYER_VIEW_H */