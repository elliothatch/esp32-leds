#include "ws2812-view.h"

#include "freertos/FreeRTOS.h"
#include <math.h>
#include <string.h>

#include "../ws2812_control.h"

struct led_state ledState;

const uint8_t gamma8[] = {    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };

fp_frameid fp_ws2812_view_get_frame(fp_view* view) {
	return ((fp_ws2812_view_data*)view->data)->frame;
}

bool fp_ws2812_view_render(fp_view* view) {
	fp_ws2812_view_data* screenData = view->data;
	fp_frame* frame = fp_frame_get(screenData->frame);
	fp_frame* childFrame = fp_frame_get(fp_view_get_frame(screenData->childView));

	if(screenData->childView != 0) {
		/* apply gamma */
		for(int i = 0; i < frame->length && i < childFrame->length; i++) {
			rgb_color color = childFrame->pixels[i];

			frame->pixels[i].fields.r = gamma8[(int)(color.fields.r * screenData->brightness)];
			frame->pixels[i].fields.g = gamma8[(int)(color.fields.g * screenData->brightness)];
			frame->pixels[i].fields.b = gamma8[(int)(color.fields.b * screenData->brightness)];
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


fp_viewid fp_create_ws2812_view(unsigned int width, unsigned int height) {
	fp_ws2812_view_data * screenData = malloc(sizeof(fp_ws2812_view_data));
	if(!screenData) {
		printf("error: fp_create_ws2812_view: failed to allocate memory for ws2812Data\n");
		return 0;
	}

	screenData->frame = fp_create_frame(width, height, rgb(0,0,0));
	screenData->childView = 0;
	screenData->brightness = 1.0f;

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
