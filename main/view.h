#ifndef VIEW_H
#define VIEW_H

#include <stdbool.h>

#include "frame.h"

/* fp: fresh pixel */

typedef unsigned int fp_viewid;

typedef enum {
	FP_VIEW_FRAME, /* just a frame */
	FP_VIEW_WS2812, /* represents the physical LED screen. rendering will render to the LEDs  */
	FP_VIEW_ANIM,
	FP_VIEW_LAYER,
	FP_VIEW_TRANSITION,
	FP_VIEW_DYNAMIC,
	FP_VIEW_TYPE_COUNT
} fp_view_type;

typedef void fp_view_data;

typedef struct {
	fp_view_type type;
	fp_viewid id;
	fp_viewid parent;
	bool dirty; /* render should be called on this before fp_frame_get */
	bool composite; /* on free_view all child views and frames are freed */
	fp_view_data* data;
} fp_view;

bool fp_view_init(unsigned int capacity);

fp_viewid fp_view_create(fp_view_type type, bool composite, fp_view_data* data); /* used internally */
bool fp_view_free(fp_viewid id);

fp_view* fp_view_get(fp_viewid id);
fp_frameid fp_view_get_frame(fp_viewid id);
void fp_view_mark_dirty(fp_viewid id);
bool fp_view_render(fp_viewid id);
bool fp_view_onnext_render(fp_viewid id);

/* fp_view_type fp_view_register_type(render_func, get_view_frame_func, pending_view_update_func) */

typedef struct {
	fp_frameid (*get_view_frame) (fp_view*);
	bool (*render_view) (fp_view*);
	bool (*onnext_render) (fp_view*); /* rename... */
	bool (*free_view) (fp_view*); /* clean up any memory the view has allocated itself. */
} fp_view_register_data;

/* TODO: do this differently */
bool fp_view_register_type(fp_view_type viewType, fp_view_register_data registerData);

/** screen view is just a buffered frame view. on render it explicitly makes a copy of its child view's primary frame*/

#endif /* VIEW_H */
