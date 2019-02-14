#ifndef PIXEL_H
#define PIXEL_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "color.h"

/* fp: fresh pixel */

/** freeRTOS task that constantly renders at given framerate */
void fp_task_render(void *pvParameters);

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
fp_frame fp_get_frame(fp_frameid id);

bool fp_fset_rect(
	fp_frameid id,
	unsigned int x,
	unsigned int y,
	/* pointer to the frame to copy from */
	fp_frame* frame
);

bool fp_fill_rect(
	fp_frameid id,
	unsigned int x,
	unsigned int y,
	unsigned int width,
	unsigned int height,
	rgb_color color
);

bool fp_render(fp_frameid id);

#endif /* PIXEL_H */
