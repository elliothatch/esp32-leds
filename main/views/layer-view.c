#include "layer-view.h"

#include "freertos/FreeRTOS.h"

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
