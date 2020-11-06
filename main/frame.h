#ifndef FRAME_H
#define FRAME_H

#include <stdbool.h>

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

bool fp_frame_init(unsigned int capacity);

/* creates a frame with given width and height, and returns its id
 * if the frame could not be created, returns id 0, which points to the NULL frame (all fields 0) */
fp_frameid fp_frame_create(unsigned int width, unsigned int height, rgb_color color);

bool fp_frame_free(fp_frameid frame);

/* retrieve the frame. if there is no frame with the id, returns the NULL frame (all values 0)
 * only use if you cannot achieve what you need with the other commands */
fp_frame* fp_frame_get(fp_frameid id);

unsigned int fp_frame_height(fp_frame* frame);
bool fp_frame_has_point(fp_frame* frame, int x, int y);
unsigned int fp_fcalc_index(unsigned int x, unsigned int y, unsigned int width);

bool fp_fset(
	fp_frameid id,
	unsigned int x,
	unsigned int y,
	rgb_color color
);

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

#endif /* FRAME_H */
