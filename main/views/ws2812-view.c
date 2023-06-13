#include "ws2812-view.h"

#include "freertos/FreeRTOS.h"
#include <math.h>
#include <string.h>

#include "../ws2812_control.h"

struct led_state ledState;

fp_frameid fp_ws2812_view_get_frame(fp_view* view) {
	return ((fp_ws2812_view_data*)view->data)->frame;
}

bool fp_ws2812_view_render(fp_view* view) {
	fp_ws2812_view_data* screenData = view->data;
	fp_frame* frame = fp_frame_get(screenData->frame);
	fp_frame* childFrame = fp_frame_get(fp_view_get_frame(screenData->childView));

	if(screenData->childView != 0) {
		/* apply gamma and indexing */
		for(int i = 0; i < frame->length && i < childFrame->length; i++) {
			rgb_color color = childFrame->pixels[i];
			unsigned int index;
			switch(screenData->indexMode) {
			case FP_INDEX_ZIGZAG:
				/*
				00 01 02 03 04 05 06 07
				15 14 13 12 11 10 09 08
				16 17 18 19 20 21 22 23
				31 30 29 28 27 26 25 24
				32 33 34 35 36 37 38 39
				47 46 45 44 43 42 41 40
				48 49 50 51 52 53 54 55
				63 62 61 60 59 58 57 56
				*/
				if((i/frame->width) % 2 == 0) {
					index = i;
				}
				else {
					index = (i/frame->width)*frame->width + (frame->width - 1 - (i % frame->width));
				}
				break;
			case FP_INDEX_GRID:
			default:
				index = i;
			}

			frame->pixels[index].fields.r = (uint8_t)(color.fields.r * screenData->brightness);
			frame->pixels[index].fields.g = (uint8_t)(color.fields.g * screenData->brightness);
			frame->pixels[index].fields.b = (uint8_t)(color.fields.b * screenData->brightness);

			/* frame->pixels[i].fields.r = gamma8[(int)(color.fields.r * screenData->brightness)]; */
			/* frame->pixels[i].fields.g = gamma8[(int)(color.fields.g * screenData->brightness)]; */
			/* frame->pixels[i].fields.b = gamma8[(int)(color.fields.b * screenData->brightness)]; */
		}

		/*
		fp_fset_rect(
			screenData->frame,
			0, 0,
			fp_frame_get(fp_view_get_frame(screenData->childView))
		);
		*/
	}

	fp_render_leds_ws2812(screenData->frame);

	return true;
}

bool fp_ws2812_view_onnext_render(fp_view* view) {
	return true;
}


fp_viewid fp_create_ws2812_view(unsigned int width, unsigned int height, fp_index_mode indexMode) {
	fp_ws2812_view_data * screenData = malloc(sizeof(fp_ws2812_view_data));
	if(!screenData) {
		printf("error: fp_create_ws2812_view: failed to allocate memory for ws2812Data\n");
		return 0;
	}

	screenData->frame = fp_frame_create(width, height, rgb(0,0,0));
	screenData->childView = 0;
	screenData->brightness = 1.0f;
	screenData->indexMode = indexMode;

	return fp_view_create(FP_VIEW_WS2812, false, screenData);
}

/* TODO: this makes some unnecessary copies and hardcodes one fixed-size LED array */
bool fp_render_leds_ws2812(fp_frameid id) {
	fp_frame* frame = fp_frame_get(id);

	/* printf("render %d, %d, %d\n", id, frame->width, frame->length); */
	/* for(int row = 0; row < frame->length / frame->width; row++) { */
	/* 	for(int col = 0; col < frame->width; col++) { */
	/* 		unsigned int idx = calc_index(row, col, frame->width); */
	/* 		printf("%03d %03d %03d | ", frame->pixels[idx].fields.r, frame->pixels[idx].fields.g, frame->pixels[idx].fields.b); */
	/* 	} */
	/* 	printf("\n"); */
	/* } */
	memcpy(ledState.leds, frame->pixels, fmin(frame->length, NUM_LEDS) * sizeof(((fp_frame*)0)->pixels));
	ws2812_write_leds(ledState);

	return true;
}

bool fp_ws2812_view_free(fp_view* view) {
	fp_ws2812_view_data* screenData = view->data;

	fp_frame_free(screenData->frame);
	free(screenData);

	return true;
}

void fp_ws2812_view_set_child(fp_viewid parent, fp_viewid child) {
	fp_view* parentView = fp_view_get(parent);
	fp_view* childView = fp_view_get(child);

	fp_ws2812_view_data* parentViewData = parentView->data;

	parentViewData->childView = child;
	childView->parent = parent;
}
