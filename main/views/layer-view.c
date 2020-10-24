#include "layer-view.h"

#include "freertos/FreeRTOS.h"

#include "frame-view.h"

/**
 * @param views - if NULL, a frame_view will be created for each layer.
 *					otherwise, an array of length layerCount with views to to be used for each layer. if an element is 0 a frame_view will be created.
 * @param layerWidth - width of created frames
 * @param layerHeight - height of created frames
 */
fp_viewid fp_create_layer_view(fp_viewid* views, unsigned int layerCount, unsigned int width, unsigned int height, unsigned int layerWidth, unsigned int layerHeight) {
	fp_layer* layers = malloc(layerCount * sizeof(fp_layer));
	if(!layers) {
		printf("error: fp_create_layer_view: failed to allocate memory for layers\n");
		return 0;
	}

	fp_view_layer_data* layerData = malloc(sizeof(fp_view_layer_data));
	if(!layerData) {
		printf("error: fp_create_layer_view: failed to allocate memory for layerData\n");
		free(layers);
		return 0;
	}

	layerData->layerCount = layerCount;
	layerData->layers = layers;
	layerData->frame = fp_create_frame(width, height, rgb(0,0,0));

	fp_viewid id = fp_create_view(FP_VIEW_LAYER, 0, layerData);

	/* init layers */
	for(int i = 0; i < layerCount; i++) {

		fp_viewid layerView = 0;

		if(views == NULL || views[i] == 0) {
			layerView = fp_create_frame_view(layerWidth, layerHeight, rgb(0,0,0));
		}
		else {
			layerView = views[i];
		}

		fp_layer layer = {
			layerView,
			FP_BLEND_REPLACE,
			0,
			0,
			255
		};
		layers[i] = layer;
		(&viewPool[layer.view])->parent = id;
	}

	return id;
}

fp_frameid fp_layer_view_get_frame(fp_view* view) {
	return ((fp_view_layer_data*)view->data)->frame;
}

bool fp_layer_view_render(fp_view* view) {
	fp_view_layer_data* layerData = view->data;
	// clear
	fp_frame* layerFrame = fp_get_frame(layerData->frame);
	fp_ffill_rect(
			layerData->frame,
			0, 0,
			layerFrame->width, layerFrame->length / layerFrame->width,
			rgb(0, 0, 0)
			);

	// draw higher indexed layers last
	for(int i = 0; i < layerData->layerCount; i++) {
		switch(layerData->layers[i].blendMode) {
			case FP_BLEND_OVERWRITE:
				fp_fset_rect(
						layerData->frame,
						layerData->layers[i].offsetX,
						layerData->layers[i].offsetY,
						fp_get_frame(fp_get_view_frame(layerData->layers[i].view))
						);
				break;
			case FP_BLEND_REPLACE:
				fp_fset_rect_transparent(
						layerData->frame,
						layerData->layers[i].offsetX,
						layerData->layers[i].offsetY,
						fp_get_frame(fp_get_view_frame(layerData->layers[i].view))
						);
				break;
			case FP_BLEND_ADD:
				fp_fadd_rect(
						layerData->frame,
						layerData->layers[i].offsetX,
						layerData->layers[i].offsetY,
						fp_get_frame(fp_get_view_frame(layerData->layers[i].view))
						);
				break;
			case FP_BLEND_MULTIPLY:
				fp_fmultiply_rect(
						layerData->frame,
						layerData->layers[i].offsetX,
						layerData->layers[i].offsetY,
						fp_get_frame(fp_get_view_frame(layerData->layers[i].view))
						);
				break;
			case FP_BLEND_ALPHA:
				{
					uint8_t srcAlpha = layerData->layers[i].alpha;
					fp_fblend_rect(
							&rgb_alpha,
							layerData->frame,
							255,
							layerData->layers[i].offsetX,
							layerData->layers[i].offsetY,
							fp_get_frame(fp_get_view_frame(layerData->layers[i].view)),
							srcAlpha
							);
					break;
				}
		}
	}

	return true;
}

bool fp_layer_view_onnext_render(fp_view* view) {
	return true;
}

