#include "frame.h"

#include <math.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

unsigned int fp_calc_index(unsigned int x, unsigned int y, unsigned int width) {
	return y * width + x % width;
}

unsigned int framePoolCount = 1;
fp_frame framePool[FP_FRAME_COUNT] = {{ 0, 0, NULL}};

/** locks fp_create_frame. allows multiple tasks to safely create frames */
SemaphoreHandle_t createFrameLock = NULL;

fp_frameid fp_create_frame(unsigned int width, unsigned int height, rgb_color color) {
	if(framePoolCount >= FP_FRAME_COUNT) {
		printf("error: fp_create_frame: frame pool full. limit: %d\n", FP_FRAME_COUNT);
		return 0;
	}

	unsigned int length = width * height;

	rgb_color* pixels = malloc(length * sizeof(rgb_color));
	if(!pixels) {
		printf("error: fp_create_frame: failed to allocate memory for pixels\n");
		return 0;
	}

	for(unsigned int i = 0; i < length; i++) {
		pixels[i] = color;
	}


	if(!createFrameLock) {
		createFrameLock = xSemaphoreCreateBinary();
		if(!createFrameLock) {
			printf("error: fp_create_frame: failed to create semaphore\n");
		}
		xSemaphoreGive(createFrameLock);
	}

	xSemaphoreTake(createFrameLock, portMAX_DELAY);
	fp_frameid id = framePoolCount++;
	xSemaphoreGive(createFrameLock);

	framePool[id].length = length;
	framePool[id].width = width;
	framePool[id].pixels = pixels;

	/* printf("create frame: %d %d %d\n", */ 
	/* 	id, */
	/* 	framePool[id].length, */
	/* 	framePool[id].width */
	/* ); */

	if(DEBUG) {
		printf("create frame %d: length: %d\n", id, length);
	}

	return id;
}

fp_frame* fp_get_frame(fp_frameid id) {
	if(id >= framePoolCount) {
		printf("error: fp_get_frame: id %d too large, max id: %d\n", id, framePoolCount-1);
		return &framePool[0];
	}

	return &framePool[id];
}

unsigned int fp_frame_height(fp_frame* frame) {
	return frame->length / frame->width;
}

bool fp_fset_rect(
		fp_frameid id,
		unsigned int x,
		unsigned int y,
		fp_frame* frame
		) {
	if(id >= framePoolCount) {
		return false;
	}

	fp_frame* targetFrame = &framePool[id];

	for(int row = 0; row < fmin(frame->length / frame->width, fmax(0, targetFrame->length / targetFrame->width - y)); row++) {
		memcpy(
			&targetFrame->pixels[fp_calc_index(x, y + row, targetFrame->width)],
			&frame->pixels[fp_calc_index(0, row, frame->width)],
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
	if(id >= framePoolCount) {
		return false;
	}

	fp_frame* frame = &framePool[id];
	for(int row = 0; row < fmin(height, fmax(0,frame->length / frame->width - y)); row++) {
		for(int col = 0; col < fmin(width, fmax(0, frame->width - x)); col++) {
			frame->pixels[fp_calc_index(x + col, y + row, frame->width)] = color;
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
	if(id >= framePoolCount) {
		return false;
	}

	fp_frame* targetFrame = &framePool[id];

	for(int row = 0; row < fmin(frame->length / frame->width, fmax(0, targetFrame->length / targetFrame->width - y)); row++) {
		for(int col = 0; col < fmin(frame->width, fmax(0, targetFrame->width - x)); col++) {
			rgb_color pixel = frame->pixels[fp_calc_index(col, row, frame->width)];
			if(pixel.fields.b == 0
				&& pixel.fields.r == 0
				&& pixel.fields.g == 0) {
				continue;
			}
			targetFrame->pixels[fp_calc_index(x + col, y + row, targetFrame->width)] = pixel;
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
	if(id >= framePoolCount) {
		return false;
	}

	fp_frame* targetFrame = &framePool[id];

	for(int row = 0; row < fmin(frame->length / frame->width, (targetFrame->length / targetFrame->width) - y); row++) {
		for(int col = 0; col < fmin(frame->width, targetFrame->width - x); col++) {
			rgb_color colorResult = (*blendFn)(
					frame->pixels[fp_calc_index(col, row, frame->width)],
					alphaSrc,
					targetFrame->pixels[fp_calc_index(x + col, y + row, targetFrame->width)],
					alphaTarget
			);
			targetFrame->pixels[fp_calc_index(x + col, y + row, targetFrame->width)] = colorResult;
		}
	}

	return true;
}
