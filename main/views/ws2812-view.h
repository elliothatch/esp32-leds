#ifndef WS2812_VIEW_H
#define WS2812_VIEW_H

#include <stdbool.h>

#include "../view.h"

/* index mode changes the order of the pixels to match different types of displays */
typedef enum {
	FP_INDEX_GRID, /* 012/345/678 */
	FP_INDEX_ZIGZAG, /* 012/543/678 */
} fp_index_mode;

/* fp: fresh pixel */
typedef struct {
	fp_viewid childView;
	fp_frameid frame;
	float brightness;
	fp_index_mode indexMode;
	/* struct led_state leds; */
} fp_ws2812_view_data;

/** screen view is just a buffered frame view. on render it explicitly makes a copy of its child view's primary frame*/
fp_viewid fp_create_ws2812_view(unsigned int width, unsigned int height, fp_index_mode indexMode);

fp_frameid fp_ws2812_view_get_frame(fp_view* view);
bool fp_ws2812_view_render(fp_view* view);
bool fp_ws2812_view_onnext_render(fp_view* view);
bool fp_ws2812_view_free(fp_view* view);

void fp_ws2812_view_set_child(fp_viewid parent, fp_viewid child);
bool fp_render_leds_ws2812(fp_frameid id);

static const fp_view_register_data fp_ws2812_view_register_data = {
	&fp_ws2812_view_get_frame,
	&fp_ws2812_view_render,
	&fp_ws2812_view_onnext_render,
	&fp_ws2812_view_free
};

#endif /* WS2812_VIEW_H */
