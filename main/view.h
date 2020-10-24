#ifndef VIEW_H
#define VIEW_H

#include <stdbool.h>

#include "frame.h"

#define FP_VIEW_COUNT 512

/* fp: fresh pixel */

typedef unsigned int fp_viewid;

typedef enum {
	FP_VIEW_FRAME, /* just a frame */
	FP_VIEW_WS2812, /* represents the physical LED screen. rendering will render to the LEDs  */
	FP_VIEW_ANIM,
	FP_VIEW_LAYER,
	FP_VIEW_TRANSITION,
	FP_VIEW_TYPE_COUNT
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

typedef void fp_view_data;

typedef struct {
	fp_view_type type;
	fp_viewid id;
	fp_viewid parent;
	bool dirty; /* render should be called on this before fp_get_frame */
	fp_view_data* data;
} fp_view;

fp_viewid fp_create_view(fp_view_type type, fp_viewid parent, fp_view_data* data); /* used internally */

fp_view* fp_get_view(fp_viewid id);
fp_frameid fp_get_view_frame(fp_viewid id);
void fp_mark_view_dirty(fp_viewid id);
bool fp_render_view(fp_viewid id);
bool fp_onnext_render(fp_viewid id);

extern fp_view viewPool[FP_VIEW_COUNT];

/* fp_view_type fp_register_view_type(render_func, get_view_frame_func, pending_view_update_func) */

typedef struct {
	fp_frameid (*get_view_frame) (fp_view*);
	bool (*render_view) (fp_view*);
	bool (*onnext_render) (fp_view*); /* rename... */
} fp_view_register_data;

/* TODO: do this differently */
bool fp_register_view_type(fp_view_type viewType, fp_view_register_data registerData);

/** screen view is just a buffered frame view. on render it explicitly makes a copy of its child view's primary frame*/

#endif /* VIEW_H */
