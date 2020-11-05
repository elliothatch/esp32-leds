#include "frame.h"

#include <math.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "pool.h"
#include "global.h"

unsigned int fp_fcalc_index(unsigned int x, unsigned int y, unsigned int width) {
	return y * width + x % width;
}

fp_pool* framePool = NULL;
fp_frame* zeroFrame;

bool fp_frame_init(unsigned int capacity) {
	framePool = fp_pool_init(capacity, sizeof(fp_frame), true);
	zeroFrame = fp_pool_get(framePool, 0);
	zeroFrame->length = 0;
	zeroFrame->width = 0;
	zeroFrame->pixels = NULL;

	return true;
}

/* fp_frame framePool[FP_FRAME_COUNT] = {{ 0, 0, NULL}}; */

/** locks fp_create_frame. allows multiple tasks to safely create frames */
SemaphoreHandle_t createFrameLock = NULL;

fp_frameid fp_create_frame(unsigned int width, unsigned int height, rgb_color color) {
	fp_frameid id = fp_pool_add(framePool);
	if(id == 0) {
		printf("error: fp_create_frame: failed to add frame\n");
		return 0;
	}

	unsigned int length = width * height;

	rgb_color* pixels = malloc(length * sizeof(rgb_color));
	if(!pixels) {
		printf("error: fp_create_frame: failed to allocate memory for pixels\n");
		fp_pool_delete(framePool, id);
		return 0;
	}

	for(unsigned int i = 0; i < length; i++) {
		pixels[i] = color;
	}

	fp_frame* frame = fp_pool_get(framePool, id);
	frame->length = length;
	frame->width = width;
	frame->pixels = pixels;

#ifdef DEBUG
		printf("frame: create %d (%d/%d): length: %d\n", id, framePool->count, framePool->capacity, length);
#endif

	return id;
}

bool fp_frame_free(fp_frameid id) {
	if(id == 0) {
		return false;
	}

	fp_frame* frame = fp_pool_get(framePool, id);
	if(frame == NULL) {
		return false;
	}

#ifdef DEBUG
		printf("frame: delete %d (%d/%d)\n", id, framePool->count, framePool->capacity);
#endif

	free(frame->pixels);
	return fp_pool_delete(framePool, id);
}

fp_frame* fp_frame_get(fp_frameid id) {
	return fp_pool_get(framePool, id);
	/* if(id >= framePoolCount) { */
	/* 	printf("error: fp_frame_get: id %d too large, max id: %d\n", id, framePoolCount-1); */
	/* 	return &framePool[0]; */
	/* } */

	/* return &framePool[id]; */
}

unsigned int fp_frame_height(fp_frame* frame) {
	return frame->length / frame->width;
}

bool fp_fset(
	fp_frameid id,
	unsigned int x,
	unsigned int y,
	rgb_color color
) {
	fp_frame* frame = fp_frame_get(id);
	if(frame == NULL) {
		return false;
	}

	unsigned int index = fp_fcalc_index(x, y, frame->width);
	if(index >= frame->length) {
		return false;
	}

	frame->pixels[index] = color;
	return true;
}

bool fp_fset_rect(
		fp_frameid id,
		unsigned int x,
		unsigned int y,
		fp_frame* frame
		) {

	fp_frame* targetFrame = fp_frame_get(id);
	if(targetFrame == NULL) {
		return false;
	}

	for(int row = 0; row < fmin(frame->length / frame->width, fmax(0, targetFrame->length / targetFrame->width - y)); row++) {
		memcpy(
			&targetFrame->pixels[fp_fcalc_index(x, y + row, targetFrame->width)],
			&frame->pixels[fp_fcalc_index(0, row, frame->width)],
			fmin(frame->width, fmax(0, targetFrame->width - x)) * sizeof(((fp_frame*)0)->pixels)
		);
	}

	return true;
}

bool fp_ffill_rect(
		fp_frameid id,
		unsigned int x,
		unsigned int y,
		unsigned int width,
		unsigned int height,
		rgb_color color
		) {

	fp_frame* frame = fp_frame_get(id);
	if(frame == NULL) {
		return false;
	}

	for(int row = 0; row < fmin(height, fmax(0,frame->length / frame->width - y)); row++) {
		for(int col = 0; col < fmin(width, fmax(0, frame->width - x)); col++) {
			frame->pixels[fp_fcalc_index(x + col, y + row, frame->width)] = color;
		}
	}

	return true;
}

/** copies a frame onto another, ignoring 0,0,0 colors */
bool fp_fset_rect_transparent(
		fp_frameid id,
		unsigned int x,
		unsigned int y,
		fp_frame* frame
		) {
	fp_frame* targetFrame = fp_frame_get(id);
	if(targetFrame == NULL) {
		return false;
	}

	for(int row = 0; row < fmin(frame->length / frame->width, fmax(0, targetFrame->length / targetFrame->width - y)); row++) {
		for(int col = 0; col < fmin(frame->width, fmax(0, targetFrame->width - x)); col++) {
			rgb_color pixel = frame->pixels[fp_fcalc_index(col, row, frame->width)];
			if(pixel.fields.b == 0
				&& pixel.fields.r == 0
				&& pixel.fields.g == 0) {
				continue;
			}
			targetFrame->pixels[fp_fcalc_index(x + col, y + row, targetFrame->width)] = pixel;
		}
	}

	return true;
}

bool fp_fadd_rect(
	fp_frameid id,
	unsigned int x,
	unsigned int y,
	/* pointer to the frame to copy from */
	fp_frame* frame
) {
	return fp_fblend_rect(&rgb_addb, id, 255, x, y, frame, 255);
}

bool fp_fmultiply_rect(
	fp_frameid id,
	unsigned int x,
	unsigned int y,
	/* pointer to the frame to copy from */
	fp_frame* frame
) {
	return fp_fblend_rect(&rgb_multiplyb, id, 255, x, y, frame, 255);
}

bool fp_fblend_rect(
	blend_fn blendFn,
	fp_frameid id,
	uint8_t alphaTarget,
	unsigned int x,
	unsigned int y,
	fp_frame* frame,
	uint8_t alphaSrc
) {
	fp_frame* targetFrame = fp_frame_get(id);
	if(targetFrame == NULL) {
		return false;
	}

	for(int row = 0; row < fmin(frame->length / frame->width, (targetFrame->length / targetFrame->width) - y); row++) {
		for(int col = 0; col < fmin(frame->width, targetFrame->width - x); col++) {
			rgb_color colorResult = (*blendFn)(
					frame->pixels[fp_fcalc_index(col, row, frame->width)],
					alphaSrc,
					targetFrame->pixels[fp_fcalc_index(x + col, y + row, targetFrame->width)],
					alphaTarget
			);
			targetFrame->pixels[fp_fcalc_index(x + col, y + row, targetFrame->width)] = colorResult;
		}
	}

	return true;
}
