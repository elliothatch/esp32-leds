#ifndef PIXEL_H
#define PIXEL_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "color.h"

/* fp: fresh pixel */

/* A frame is buffer storing color information for a 2D frame. */
typedef struct {
	/* length of "pixels" buffer */
	unsigned int length;
	/* width of the 2D frame in pixels */
	unsigned int width;
	rgb_color* pixels;
} fp_frame;

typedef unsigned int fp_frameid;

/* creates a frame with given width and height, and returns its id
 * if the frame could not be created, returns id 0, which points to the NULL frame (all fields 0) */
fp_frameid fp_create_frame(unsigned int width, unsigned int height, rgb_color color);

/* retrieve the frame. if there is no frame with the id, returns the NULL frame (all values 0)
 * only use if you cannot achieve what you need with the other commands */
fp_frame* fp_get_frame(fp_frameid id);

bool fp_fset_rect(
	fp_frameid id,
	unsigned int x,
	unsigned int y,
	/* pointer to the frame to copy from */
	fp_frame* frame
);

bool fp_fset_rect_transparent(
	fp_frameid id,
	unsigned int x,
	unsigned int y,
	/* pointer to the frame to copy from */
	fp_frame* frame
);

bool fp_ffill_rect(
	fp_frameid id,
	unsigned int x,
	unsigned int y,
	unsigned int width,
	unsigned int height,
	rgb_color color
);

/** combines the frames using rgb elementwise addition */
bool fp_fadd_rect(
	fp_frameid id,
	unsigned int x,
	unsigned int y,
	/* pointer to the frame to copy from */
	fp_frame* frame
);

/** combines the frames using rgb elementwise multiplication */
bool fp_fmultiply_rect(
	fp_frameid id,
	unsigned int x,
	unsigned int y,
	/* pointer to the frame to copy from */
	fp_frame* frame
);

/* blends the frames using an elementwise blend function with constant alpha values for each frame
 * TODO: add rgba color frames and corresponding blend function for per-pixel alpha?
 */
bool fp_fblend_rect(
	blend_fn blendFn,
	fp_frameid id,
	/** alpha used for the frame we are copying to */
	uint8_t alphaTarget,
	unsigned int x,
	unsigned int y,
	/* pointer to the frame to copy from */
	fp_frame* frame,
	/** alpha used for the frame we are copying from */
	uint8_t alphaSrc

);

bool fp_render(fp_frameid id);

typedef enum fp_command{
	SET_RECT,
	FILL_RECT,
	RENDER,
	RENDER_VIEW
} fp_command;

typedef unsigned int fp_viewid;

typedef union {
	struct {
		fp_frameid id;
		unsigned int x;
		unsigned int y;
		fp_frame* frame;
	} SET_RECT;
	struct {
		fp_frameid id;
		unsigned int x;
		unsigned int y;
		unsigned int width;
		unsigned int height;
		rgb_color color;
	} FILL_RECT;
	struct {
		fp_frameid id;
	} RENDER;
	struct {
		fp_viewid id;
	} RENDER_VIEW;
} fp_fargs;

typedef struct {
	fp_command cmd;
	fp_fargs fargs;
} fp_queue_command;

typedef enum {
	FP_VIEW_FRAME, /* just a frame */
	FP_VIEW_SCREEN, /* represents the physical LED screen. rendering will render to the LEDs  */
	FP_VIEW_ANIM,
	FP_VIEW_LAYER
} fp_view_type;

typedef struct {
	fp_frameid frame;
} fp_view_frame_data;

typedef struct {
	fp_viewid childView;
	fp_frameid frame;
	/* struct led_state leds; */
} fp_view_screen_data;

typedef struct {
	unsigned int frameCount;
	fp_viewid* frames;
	unsigned int frameIndex;
	unsigned int frameratePeriodMs;
} fp_view_anim_data;

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

typedef struct {
	/* mapViews are anim_views where each frame contains data (mapFields) about how one view is mapped onto the transition
	 */
	fp_viewid mapViewA;
	fp_viewid mapViewB;
	rgb_color (*blend_fn)(rgb_color a, uint16_t aWeight, rgb_color b, uint16_t bWeight);
	/** stores the result of render */
	fp_frameid frame;

} fp_view_transition_data;

typedef union {
	fp_view_frame_data* FRAME;
	fp_view_screen_data* SCREEN;
	fp_view_anim_data* ANIM;
	fp_view_layer_data* LAYER;
	fp_view_transition_data* TRANSITION;
} fp_view_data;

typedef struct {
	fp_view_type type;
	fp_viewid parent;
	bool dirty; /* render should be called on this before fp_get_frame */
	fp_view_data data;
} fp_view;

fp_viewid fp_create_view(fp_view_type type, fp_viewid parent, fp_view_data data); /* used internally */

fp_viewid fp_create_frame_view(unsigned int width, unsigned int height, rgb_color color);
fp_viewid fp_create_screen_view(unsigned int width, unsigned int height);
fp_viewid fp_create_anim_view(fp_viewid* views, unsigned int frameCount, unsigned int frameratePeriodMs, unsigned int width, unsigned int height);
fp_viewid fp_create_layer_view(fp_viewid* views, unsigned int layerCount, unsigned int width, unsigned int height, unsigned int layerWidth, unsigned int layerHeight);

fp_view* fp_get_view(fp_viewid id);
bool fp_render_view(fp_viewid id);
void fp_mark_view_dirty(fp_viewid id);

// returns a frame containing the contents of that view. compositing views (e.g. layer_view) may 
fp_frameid fp_get_view_frame(fp_viewid id);

/** freeRTOS task that constantly renders at given framerate */
typedef struct {
	int refresh_period_ms;
	fp_viewid rootView;
	QueueHandle_t commands;
	/* prevents shutting down the chip while rendering, which can cause bright flashes */
	SemaphoreHandle_t shutdownLock;
} fp_task_render_params;

void fp_task_render(void *pvParameters);


#endif /* PIXEL_H */
