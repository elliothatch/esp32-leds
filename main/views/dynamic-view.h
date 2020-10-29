#ifndef DYNAMIC_VIEW_H
#define DYNAMIC_VIEW_H

#include <stdbool.h>

#include "../view.h"

/* fp: fresh pixel */
/* dynamic view is recomputed on each fp_view_render by invoking a custom callback function
 * TODO: shouldn't store a frame, to minimize memory usage
 * */

typedef struct {
	fp_frameid frame;
	bool (*renderFunc) (fp_view*);
	bool (*onnextRenderFunc) (fp_view*);
	/** custom data */
	void* data;
} fp_dynamic_view_data;

fp_viewid fp_dynamic_view_create(
	unsigned int width,
	unsigned int height,
	bool (*renderFunc) (fp_view*),
	bool (*onnextRenderFunc) (fp_view*),
	void* data
);

fp_frameid fp_dynamic_view_get_frame(fp_view* view);
bool fp_dynamic_view_render(fp_view* view);
bool fp_dynamic_view_onnext_render(fp_view* view);
bool fp_dynamic_view_free(fp_view* view);

static const fp_view_register_data fp_dynamic_view_register_data = {
	&fp_dynamic_view_get_frame,
	&fp_dynamic_view_render,
	&fp_dynamic_view_onnext_render,
	&fp_dynamic_view_free
};

#endif /* DYNAMIC_VIEW_H */
