#ifndef VIEW_H
#define VIEW_H

#include <stdbool.h>

#include "frame.h"

#define FP_VIEW_COUNT 512

/* fp: fresh pixel */

typedef unsigned int fp_viewid;

typedef enum {
	FP_VIEW_FRAME, /* just a frame */
	FP_VIEW_SCREEN, /* represents the physical LED screen. rendering will render to the LEDs  */
	FP_VIEW_ANIM,
	FP_VIEW_LAYER,
	FP_VIEW_TRANSITION
} fp_view_type;

/*
typedef union {
	fp_view_frame_data* FRAME;
	fp_view_screen_data* SCREEN;
	fp_view_anim_data* ANIM;
	fp_view_layer_data* LAYER;
	fp_view_transition_data* TRANSITION;
} fp_view_data;
*/

typedef void* fp_view_data;

typedef struct {
	fp_view_type type;
	fp_viewid parent;
	bool dirty; /* render should be called on this before fp_get_frame */
	fp_view_data data;
} fp_view;

fp_viewid fp_create_view(fp_view_type type, fp_viewid parent, fp_view_data data); /* used internally */

fp_view* fp_get_view(fp_viewid id);
void fp_mark_view_dirty(fp_viewid id);

extern fp_view viewPool[FP_VIEW_COUNT];


/* frame view */
typedef struct {
	fp_frameid frame;
} fp_view_frame_data;

typedef struct {
	fp_viewid childView;
	fp_frameid frame;
	/* struct led_state leds; */
} fp_view_screen_data;


fp_viewid fp_create_frame_view(unsigned int width, unsigned int height, rgb_color color);

/** screen view is just a buffered frame view. on render it explicitly makes a copy of its child view's primary frame*/
fp_viewid fp_create_screen_view(unsigned int width, unsigned int height);

/** TODO: come up with a better naming scheme */
fp_viewid fp_create_frame_view_composite(fp_frameid frameid);

#endif /* VIEW_H */
