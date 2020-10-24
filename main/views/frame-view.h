#ifndef FRAME_VIEW_H
#define FRAME_VIEW_H

#include <stdbool.h>

#include "../view.h"

/* fp: fresh pixel */

/* frame view */
typedef struct {
	fp_frameid frame;
} fp_frame_view_data;

fp_viewid fp_create_frame_view(unsigned int width, unsigned int height, rgb_color color);
fp_viewid fp_create_frame_view_composite(fp_frameid frameid);

fp_frameid fp_frame_view_get_frame(fp_view* view);
bool fp_frame_view_render(fp_view* view);
bool fp_frame_view_onnext_render(fp_view* view);
bool fp_frame_view_free(fp_view* view);

static const fp_view_register_data fp_frame_view_register_data = {
	&fp_frame_view_get_frame,
	&fp_frame_view_render,
	&fp_frame_view_onnext_render,
	&fp_frame_view_free
};

#endif /* FRAME_VIEW_H */
